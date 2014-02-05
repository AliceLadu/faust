/************************************************************************
 ************************************************************************
    FAUST compiler
	Copyright (C) 2003-2004 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 ************************************************************************
 ************************************************************************/
 
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

#ifndef WIN32
#include <sys/time.h>
#include "libgen.h"
#endif

#include "global.hh"
#include "compatibility.hh"
#include "signals.hh"
#include "sigtype.hh"
#include "sigtyperules.hh"
#include "sigprint.hh"
#include "simplify.hh"
#include "privatise.hh"
#include "recursivness.hh"
#include "propagate.hh"
#include "errormsg.hh"
#include "ppbox.hh"
#include "enrobage.hh"
#include "eval.hh"
#include "description.hh"
#include "floats.hh"
#include "doc.hh"
#include "global.hh"

#include <map>
#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <fstream>
#include <sstream>

#ifndef _WIN32
#include <unistd.h>
#endif

#include "sourcereader.hh"
#include "instructions_compiler.hh"
#include "dag_instructions_compiler.hh"
#include "c_code_container.hh"
#include "cpp_code_container.hh"
#include "cpp_gpu_code_container.hh"
#include "java_code_container.hh"
#include "js_code_container.hh"
#include "asmjs_code_container.hh"
#include "llvm_code_container.hh"
#include "fir_code_container.hh"
#include "schema.h"
#include "drawschema.hh"
#include "timing.hh"
#include "ppsig.hh"
#include "garbageable.hh"
#include "export.hh"

#define FAUSTVERSION "2.0.a12"

#ifdef _WIN32
#include <windows.h>
#define PATH_MAX MAX_PATH

static char *realpath(const char *path, char resolved_path[MAX_PATH])
{
	if (GetFullPathNameA(path, MAX_PATH, resolved_path, 0)) {
		return resolved_path;
	} else {
		return "";
    }
}

#endif

// Same as libfaust.h 
typedef struct LLVMResult {
    llvm::Module*       fModule;
    llvm::LLVMContext*  fContext;
} LLVMResult;

EXPORT LLVMResult* compile_faust_llvm(int argc, const char* argv[], const char* name, const char* input, char* error_msg);
EXPORT int compile_faust(int argc, const char* argv[], const char* name, const char* input, char* error_msg);
EXPORT string expand_dsp(int argc, const char* argv[], const char* name, const char* input, char* error_msg);
EXPORT std::list<std::string> get_import_dirs();

using namespace std;

typedef void* (*compile_fun)(void* arg);

static void call_fun(compile_fun fun)
{
    pthread_t thread;
    pthread_attr_t attr; 
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 524288 * 8); 
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    pthread_create(&thread, &attr, fun, NULL);
    pthread_join(thread, NULL);
}

Tree gProcessTree;
Tree gLsignalsTree;
int gNumInputs, gNumOutputs;
string gErrorMessage;

std::list<std::string> get_import_dirs() { return gGlobal->gImportDirList; }

static Tree evaluateBlockDiagram(Tree expandedDefList, int& numInputs, int& numOutputs);

static void* thread_evaluateBlockDiagram(void* arg) 
{   
    try {
        gProcessTree = evaluateBlockDiagram(gGlobal->gExpandedDefList, gNumInputs, gNumOutputs);
    } catch (faustexception& e) {
        gErrorMessage = e.Message();
    }
    return 0;
}

static void* thread_boxPropagateSig(void* arg)
{
    try {
        gLsignalsTree = boxPropagateSig(gGlobal->nil, gProcessTree, makeSigInputList(gNumInputs));
    } catch (faustexception& e) {
        gErrorMessage = e.Message();
    }
    return 0;
}

/****************************************************************
 						Global context variable
*****************************************************************/

global* gGlobal = NULL;

/****************************************************************
 						Parser variables
*****************************************************************/
int yyerr;

/****************************************************************
 				Command line tools and arguments
*****************************************************************/

//-- command line arguments

static bool	gHelpSwitch = false;
static bool	gVersionSwitch = false;
static bool gGraphSwitch = false;
static bool gDrawPSSwitch = false;
static bool gDrawSVGSwitch = false;
static bool gPrintXMLSwitch = false;
static bool gPrintDocSwitch = false;
static int gBalancedSwitch = 0;
static const char* gArchFile = 0;
static bool gExportDSP = false;

static int gTimeout = INT_MAX;            // time out to abort compiler (in seconds)
static bool gPrintFileListSwitch = false;
static const char* gOutputLang = 0;
static bool gLLVMOut = true;

//-- command line tools

static bool isCmd(const char* cmd, const char* kw1)
{
	return 	(strcmp(cmd, kw1) == 0);
}

static bool isCmd(const char* cmd, const char* kw1, const char* kw2)
{
	return 	(strcmp(cmd, kw1) == 0) || (strcmp(cmd, kw2) == 0);
}

static bool process_cmdline(int argc, const char* argv[])
{
	int	i = 1; 
    int err = 0;
    stringstream parse_error;
    
    /*
    for (int i = 0; i < argc; i++) {
        printf("process_cmdline i = %d cmd = %s\n", i, argv[i]);
    }
    */

	while (i < argc) {

		if (isCmd(argv[i], "-h", "--help")) {
			gHelpSwitch = true;
			i += 1;
        } else if (isCmd(argv[i], "-lang", "--language") && (i+1 < argc)) {
			gOutputLang = argv[i+1];
			i += 2;
        } else if (isCmd(argv[i], "-v", "--version")) {
			gVersionSwitch = true;
			i += 1;

		} else if (isCmd(argv[i], "-d", "--details")) {
			gGlobal->gDetailsSwitch = true;
			i += 1;

		} else if (isCmd(argv[i], "-a", "--architecture") && (i+1 < argc)) {
			gArchFile = argv[i+1];
			i += 2;

		} else if (isCmd(argv[i], "-o") && (i+1 < argc)) {
			gGlobal->gOutputFile = argv[i+1];
			i += 2;

		} else if (isCmd(argv[i], "-ps", "--postscript")) {
			gDrawPSSwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-xml", "--xml")) {
            gPrintXMLSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-tg", "--task-graph")) {
            gGraphSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-sg", "--signal-graph")) {
            gGlobal->gDrawSignals = true;
            i += 1;

        } else if (isCmd(argv[i], "-blur", "--shadow-blur")) {
            gGlobal->gShadowBlur = true;
            i += 1;

		} else if (isCmd(argv[i], "-svg", "--svg")) {
			gDrawSVGSwitch = true;
			i += 1;

		} else if (isCmd(argv[i], "-f", "--fold") && (i+1 < argc)) {
			gGlobal->gFoldThreshold = atoi(argv[i+1]);
			i += 2;

		} else if (isCmd(argv[i], "-mns", "--max-name-size") && (i+1 < argc)) {
			gGlobal->gMaxNameSize = atoi(argv[i+1]);
			i += 2;

		} else if (isCmd(argv[i], "-sn", "--simple-names")) {
			gGlobal->gSimpleNames = true;
			i += 1;

		} else if (isCmd(argv[i], "-lb", "--left-balanced")) {
			gBalancedSwitch = 0;
			i += 1;

		} else if (isCmd(argv[i], "-mb", "--mid-balanced")) {
			gBalancedSwitch = 1;
			i += 1;

		} else if (isCmd(argv[i], "-rb", "--right-balanced")) {
			gBalancedSwitch = 2;
			i += 1;

		} else if (isCmd(argv[i], "-lt", "--less-temporaries")) {
			gGlobal->gLessTempSwitch = true;
			i += 1;

		} else if (isCmd(argv[i], "-mcd", "--max-copy-delay") && (i+1 < argc)) {
			gGlobal->gMaxCopyDelay = atoi(argv[i+1]);
			i += 2;

		} else if (isCmd(argv[i], "-sd", "--simplify-diagrams")) {
			gGlobal->gSimplifyDiagrams = true;
			i += 1;

        } else if (isCmd(argv[i], "-vec", "--vectorize")) {
            gGlobal->gVectorSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-vls", "--vec-loop-size") && (i+1 < argc)) {
            gGlobal->gVecLoopSize = atoi(argv[i+1]);
            i += 2;

        } else if (isCmd(argv[i], "-scal", "--scalar")) {
            gGlobal->gVectorSwitch = false;
            i += 1;

        } else if (isCmd(argv[i], "-dfs", "--deepFirstScheduling")) {
            gGlobal->gDeepFirstSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-vs", "--vec-size") && (i+1 < argc)) {
            gGlobal->gVecSize = atoi(argv[i+1]);
            i += 2;

        } else if (isCmd(argv[i], "-lv", "--loop-variant") && (i+1 < argc)) {
            gGlobal->gVectorLoopVariant = atoi(argv[i+1]);
            if (gGlobal->gVectorLoopVariant < 0 ||
                gGlobal->gVectorLoopVariant > 1) {
                stringstream error;
                error << "ERROR : invalid loop variant: \"" << gGlobal->gVectorLoopVariant <<"\"" << endl;
                throw faustexception(error.str());
            }
            i += 2;

        } else if (isCmd(argv[i], "-omp", "--openMP")) {
            gGlobal->gOpenMPSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-pl", "--par-loop")) {
            gGlobal->gOpenMPLoop = true;
            i += 1;

        } else if (isCmd(argv[i], "-sch", "--scheduler")) {
			gGlobal->gSchedulerSwitch = true;
			i += 1;

         } else if (isCmd(argv[i], "-ocl", "--openCL")) {
			gGlobal->gOpenCLSwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-cuda", "--CUDA")) {
			gGlobal->gCUDASwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-g", "--groupTasks")) {
			gGlobal->gGroupTaskSwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-fun", "--funTasks")) {
			gGlobal->gFunTaskSwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-uim", "--user-interface-macros")) {
			gGlobal->gUIMacroSwitch = true;
			i += 1;

        } else if (isCmd(argv[i], "-t", "--timeout") && (i+1 < argc)) {
            gTimeout = atoi(argv[i+1]);
            i += 2;
            
        } else if (isCmd(argv[i], "-time", "--compilation-time")) {
            gGlobal->gTimingSwitch = true;
            i += 1;

        // double float options
        } else if (isCmd(argv[i], "-single", "--single-precision-floats")) {
            gGlobal->gFloatSize = 1;
            i += 1;

        } else if (isCmd(argv[i], "-double", "--double-precision-floats")) {
            gGlobal->gFloatSize = 2;
            i += 1;

        } else if (isCmd(argv[i], "-quad", "--quad-precision-floats")) {
            gGlobal->gFloatSize = 3;
            i += 1;

        } else if (isCmd(argv[i], "-mdoc", "--mathdoc")) {
            gPrintDocSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-mdlang", "--mathdoc-lang") && (i+1 < argc)) {
            gGlobal->gDocLang = argv[i+1];
            i += 2;

        } else if (isCmd(argv[i], "-stripmdoc", "--strip-mdoc-tags")) {
            gGlobal->gStripDocSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-flist", "--file-list")) {
            gPrintFileListSwitch = true;
            i += 1;

        } else if (isCmd(argv[i], "-norm", "--normalized-form")) {
            gGlobal->gDumpNorm = true;
            i += 1;

		} else if (isCmd(argv[i], "-cn", "--class-name") && (i+1 < argc)) {
			gGlobal->gClassName = argv[i+1];
			i += 2;

        } else if (isCmd(argv[i], "-i", "--inline-architecture-files")) {
            gGlobal->gInlineArchSwitch = true;
            i += 1;
        
        } else if (isCmd(argv[i], "-e", "--export-dsp")) {
            gExportDSP = true;
            i += 1;
         
        } else if (isCmd(argv[i], "-I", "--import-dir")) {

            char temp[PATH_MAX+1];
            char* path = realpath(argv[i+1], temp);
            if (path) {
                gGlobal->gImportDirList.push_back(path);
            }
            i += 2;
            
        } else if (isCmd(argv[i], "-O", "--output-dir")) {
        
            char temp[PATH_MAX+1];
            char* path = realpath(argv[i+1], temp);
            if (path) {
                gGlobal->gOutputDir = path;
            }
            i += 2;
	
        } else if (argv[i][0] != '-') {
            const char* url = strip_start(argv[i]);
			if (check_url(url)) {
				gGlobal->gInputFiles.push_back(url);
			}
			i++;

		} else {
            if (err == 0) {
                parse_error << "unrecognized option(s) : \"" << argv[i] <<"\"";
            } else {
                parse_error << ",\"" << argv[i] <<"\"";
            }
            i++;
			err++;
		}
	}

    // adjust related options
    if (gGlobal->gOpenMPSwitch || gGlobal->gSchedulerSwitch) gGlobal->gVectorSwitch = true;
    
    if (gGlobal->gVecSize < 4) {
        stringstream error;
        error << "[-vs = "<< gGlobal->gVecSize << "] should be at least 4" << endl;
        throw faustexception(error.str());
    }

    if (gGlobal->gVecLoopSize > gGlobal->gVecSize) {
        stringstream error;
        error << "[-vls = "<< gGlobal->gVecLoopSize << "] has to be <= [-vs = " << gGlobal->gVecSize << "]" << endl;
        throw faustexception(error.str());
    }
    
    if (err != 0) {
        throw faustexception("ERROR: " + parse_error.str() + '\n');
    }

	return err == 0;
}

/****************************************************************
 					 Help and Version information
*****************************************************************/

static void printversion()
{
	cout << "FAUST: DSP to C, C++, JAVA, JavaScript, LLVM compiler, Version " << FAUSTVERSION << "\n";
	cout << "Copyright (C) 2002-2014, GRAME - Centre National de Creation Musicale. All rights reserved. \n\n";
}

static void printhelp()
{
	printversion();
	cout << "usage: faust [options] file1 [file2 ...]\n";
	cout << "\twhere options represent zero or more compiler options \n\tand fileN represents a faust source file (.dsp extension).\n";

	cout << "\noptions :\n";
	cout << "---------\n";

	cout << "-h \t\tprint this --help message\n";
	cout << "-v \t\tprint compiler --version information\n";
	cout << "-d \t\tprint compilation --details\n";
    cout << "-tg \t\tprint the internal --task-graph in dot format file\n";
    cout << "-sg \t\tprint the internal --signal-graph in dot format file\n";
    cout << "-ps \t\tprint block-diagram --postscript file\n";
    cout << "-svg \tprint block-diagram --svg file\n";
    cout << "-mdoc \tprint --mathdoc of a Faust program in LaTeX format in a -mdoc directory\n";
    cout << "-mdlang <l>\t\tload --mathdoc-lang <l> if translation file exists (<l> = en, fr, ...)\n";
    cout << "-stripdoc \t\tapply --strip-mdoc-tags when printing Faust -mdoc listings\n";
    cout << "-sd \t\ttry to further --simplify-diagrams before drawing them\n";
	cout << "-f <n> \t\t--fold <n> threshold during block-diagram generation (default 25 elements) \n";
	cout << "-mns <n> \t--max-name-size <n> threshold during block-diagram generation (default 40 char)\n";
	cout << "-sn \t\tuse --simple-names (without arguments) during block-diagram generation\n";
	cout << "-xml \t\tgenerate an --xml description file\n";
    cout << "-blur \t\tadd a --shadow-blur to SVG boxes\n";
	cout << "-lb \t\tgenerate --left-balanced expressions\n";
	cout << "-mb \t\tgenerate --mid-balanced expressions (default)\n";
	cout << "-rb \t\tgenerate --right-balanced expressions\n";
	cout << "-lt \t\tgenerate --less-temporaries in compiling delays\n";
	cout << "-mcd <n> \t--max-copy-delay <n> threshold between copy and ring buffer implementation (default 16 samples)\n";
	cout << "-a <file> \tC++ architecture file\n";
	cout << "-i \t--inline-architecture-files \n";
	cout << "-cn <name> \t--class-name <name> specify the name of the dsp class to be used instead of mydsp \n";
	cout << "-t <sec> \t--timeout <sec>, abort compilation after <sec> seconds (default 120)\n";
    cout << "-time \t--compilation-time, flag to display compilation phases timing information\n";
    cout << "-o <file> \tC++ output file\n";
    cout << "-scal   \t--scalar generate non-vectorized code\n";
    cout << "-vec    \t--vectorize generate easier to vectorize code\n";
    cout << "-vls <n>  \t--vec-loop-size size of the vector DSP loop for auto-vectorization (experimental) \n";
    cout << "-vs <n> \t--vec-size <n> size of the vector (default 32 samples)\n";
    cout << "-lv <n> \t--loop-variant [0:fastest (default), 1:simple] \n";
    cout << "-omp    \t--openMP generate OpenMP pragmas, activates --vectorize option\n";
    cout << "-pl     \t--par-loop generate parallel loops in --openMP mode\n";
    cout << "-sch    \t--scheduler generate tasks and use a Work Stealing scheduler, activates --vectorize option\n";
    cout << "-ocl    \t--openCL generate tasks with OpenCL \n";
    cout << "-cuda   \t--cuda generate tasks with CUDA \n";
	cout << "-dfs    \t--deepFirstScheduling schedule vector loops in deep first order\n";
    cout << "-g    \t\t--groupTasks group single-threaded sequential tasks together when -omp or -sch is used\n";
    cout << "-fun  \t\t--funTasks separate tasks code as separated functions (in -vec, -sch, or -omp mode)\n";
    cout << "-lang <lang> \t--language generate various output formats : c, cpp, java, js, ajs, llvm, fir (default cpp)\n";
    cout << "-uim    \t--user-interface-macros add user interface macro definitions in the C++ code\n";
    cout << "-single \tuse --single-precision-floats for internal computations (default)\n";
    cout << "-double \tuse --double-precision-floats for internal computations\n";
    cout << "-quad \t\tuse --quad-precision-floats for internal computations\n";
    cout << "-flist \t\tuse --file-list used to eval process\n";
    cout << "-norm \t\t--normalized-form prints signals in normalized form and exits\n";
    cout << "-I <dir> \t--import-dir <dir> add the directory <dir> to the import search path\n";

	cout << "\nexample :\n";
	cout << "---------\n";

	cout << "faust -a jack-gtk.cpp -o myfx.cpp myfx.dsp\n";
}

static void printheader(ostream& dst)
{
    // defines the metadata we want to print as comments at the begin of in the C++ file
    set<Tree> selectedKeys;
    selectedKeys.insert(tree("name"));
    selectedKeys.insert(tree("author"));
    selectedKeys.insert(tree("copyright"));
    selectedKeys.insert(tree("license"));
    selectedKeys.insert(tree("version"));

    dst << "//-----------------------------------------------------" << endl;
    for (MetaDataSet::iterator i = gGlobal->gMetaDataSet.begin(); i != gGlobal->gMetaDataSet.end(); i++) {
        if (selectedKeys.count(i->first)) {
            dst << "// " << *(i->first);
            const char* sep = ": ";
            for (set<Tree>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
                dst << sep << **j;
                sep = ", ";
            }
            dst << endl;
        }
    }

    dst << "//" << endl;
    dst << "// Code generated with Faust " << FAUSTVERSION << " (http://faust.grame.fr)" << endl;
    dst << "//-----------------------------------------------------" << endl;
}

/****************************************************************
 					 			MAIN
*****************************************************************/

/**
 * transform a filename "faust/example/noise.dsp" into
 * the corresponding fx name "noise"
 */
static string fxname(const string& filename)
{
	// determine position right after the last '/' or 0
	unsigned int p1 = 0;
    for (unsigned int i=0; i<filename.size(); i++) {
        if (filename[i] == '/')  { p1 = i+1; }
    }

	// determine position of the last '.'
	unsigned int p2 = filename.size();
    for (unsigned int i=p1; i<filename.size(); i++) {
        if (filename[i] == '.')  { p2 = i; }
    }

    return filename.substr(p1, p2-p1);
}

string makeDrawPath()
{
    if (gGlobal->gOutputDir != "") {
        return gGlobal->gOutputDir + "/" + gGlobal->gMasterName + ".dsp";
    } else {
        return gGlobal->gMasterDocument;
    }
}

static string makeDrawPathNoExt()
{
    if (gGlobal->gOutputDir != "") {
        return gGlobal->gOutputDir + "/" + gGlobal->gMasterName;
    } else if (gGlobal->gMasterDocument.length() >= 4 && gGlobal->gMasterDocument.substr(gGlobal->gMasterDocument.length() - 4) == ".dsp") {
        return gGlobal->gMasterDocument.substr(0, gGlobal->gMasterDocument.length() - 4);
    } else {
        return gGlobal->gMasterDocument;
    }
}

static void initFaustDirectories()
{
    char s[1024];
    getFaustPathname(s, 1024);

    gGlobal->gFaustDirectory = filedirname(s);
    gGlobal->gFaustSuperDirectory = filedirname(gGlobal->gFaustDirectory);
    gGlobal->gFaustSuperSuperDirectory = filedirname(gGlobal->gFaustSuperDirectory);
    if (gGlobal->gInputFiles.empty()) {
        gGlobal->gMasterDocument = "Unknown";
        gGlobal->gMasterDirectory = ".";
		gGlobal->gMasterName = "faustfx";
		gGlobal->gDocName = "faustdoc";
    } else {
        gGlobal->gMasterDocument = *gGlobal->gInputFiles.begin();
        gGlobal->gMasterDirectory = filedirname(gGlobal->gMasterDocument);
		gGlobal->gMasterName = fxname(gGlobal->gMasterDocument);
		gGlobal->gDocName = fxname(gGlobal->gMasterDocument);
    }
}

static void parseSourceFiles()
{
    startTiming("parser");

    list<string>::iterator s;
    gGlobal->gResult2 = gGlobal->nil;

    if (gGlobal->gInputFiles.begin() == gGlobal->gInputFiles.end()) {
        stringstream error;
        error << "ERROR : no files specified; for help type \"faust --help\"" << endl;
        throw faustexception(error.str());
    }
    for (s = gGlobal->gInputFiles.begin(); s != gGlobal->gInputFiles.end(); s++) {
        if (s == gGlobal->gInputFiles.begin()) {
            gGlobal->gMasterDocument = *s;
        }
        gGlobal->gResult2 = cons(importFile(tree(s->c_str())), gGlobal->gResult2);
    }
   
    gGlobal->gExpandedDefList = gGlobal->gReader.expandlist(gGlobal->gResult2);

    endTiming("parser");
}

static Tree evaluateBlockDiagram(Tree expandedDefList, int& numInputs, int& numOutputs)
{
    startTiming("evaluation");

    Tree process = evalprocess(expandedDefList);
    if (gGlobal->gErrorCount > 0) {
        stringstream error;
        error << "ERROR : total of " << gGlobal->gErrorCount << " errors during the compilation of " << gGlobal->gMasterDocument << ";\n";
        throw faustexception(error.str());
    }

    if (gGlobal->gDetailsSwitch) { cout << "process = " << boxpp(process) << ";\n"; }

    if (gDrawPSSwitch || gDrawSVGSwitch) {
        string projname = makeDrawPathNoExt();
        if (gDrawPSSwitch)  { drawSchema(process, subst("$0-ps",  projname).c_str(), "ps"); }
        if (gDrawSVGSwitch) { drawSchema(process, subst("$0-svg", projname).c_str(), "svg"); }
    }

    if (!getBoxType(process, &numInputs, &numOutputs)) {
        stringstream error;
        error << "ERROR during the evaluation of process : " << boxpp(process) << endl;
        throw faustexception(error.str());
    }

    if (gGlobal->gDetailsSwitch) {
        cout <<"process has " << numInputs <<" inputs, and " << numOutputs <<" outputs" << endl;
    }

    endTiming("evaluation");

    if (gPrintFileListSwitch) {
        cout << "******* ";
        // print the pathnames of the files used to evaluate process
        vector<string> pathnames = gGlobal->gReader.listSrcFiles();
        for (unsigned int i=0; i< pathnames.size(); i++) cout << pathnames[i] << ' ';
        cout << endl;
    }

    return process;
}

static pair<InstructionsCompiler*, CodeContainer*> generateCode(Tree signals, int numInputs, int numOutputs)
{
    // By default use "cpp" output
    if (!gOutputLang) {
        gOutputLang = "cpp";
    }

    InstructionsCompiler* comp = NULL;
    CodeContainer* container = NULL;

    startTiming("compilation");

    if (strcmp(gOutputLang, "llvm") == 0) {

        container = LLVMCodeContainer::createContainer(gGlobal->gClassName, numInputs, numOutputs);

        if (gGlobal->gVectorSwitch) {
            comp = new DAGInstructionsCompiler(container);
        } else {
            comp = new InstructionsCompiler(container);
        }

        if (gPrintXMLSwitch) comp->setDescription(new Description());
        if (gPrintDocSwitch) comp->setDescription(new Description());
  
        comp->compileMultiSignal(signals);
        LLVMCodeContainer* llvm_container = dynamic_cast<LLVMCodeContainer*>(container);
        gGlobal->gLLVMResult = llvm_container->produceModule(gGlobal->gOutputFile.c_str());
        
        if (gLLVMOut && gGlobal->gOutputFile == "") {
            outs() << *gGlobal->gLLVMResult->fModule;
        }
 
    } else {
    
        ostream* dst;

        if (gGlobal->gOutputFile != "") {
            string outpath = (gGlobal->gOutputDir != "") ? (gGlobal->gOutputDir + "/" + gGlobal->gOutputFile) : gGlobal->gOutputFile;
            dst = new ofstream(outpath.c_str());
        } else {
            dst = &cout;
        }
        
        if (strcmp(gOutputLang, "c") == 0) {

            container = CCodeContainer::createContainer(gGlobal->gClassName, numInputs, numOutputs, dst);

        } else if (strcmp(gOutputLang, "cpp") == 0) {

            container = CPPCodeContainer::createContainer(gGlobal->gClassName, "dsp", numInputs, numOutputs, dst);

        } else if (strcmp(gOutputLang, "java") == 0) {

            container = JAVACodeContainer::createContainer(gGlobal->gClassName, "dsp", numInputs, numOutputs, dst);
            
        } else if (strcmp(gOutputLang, "js") == 0) {

            container = JAVAScriptCodeContainer::createContainer(gGlobal->gClassName, "dsp", numInputs, numOutputs, dst);
        
        } else if (strcmp(gOutputLang, "ajs") == 0) {

            container = ASMJAVAScriptCodeContainer::createContainer(gGlobal->gClassName, "dsp", numInputs, numOutputs, dst);

        } else if (strcmp(gOutputLang, "fir") == 0) {
       
            container = FirCodeContainer::createContainer(gGlobal->gClassName, numInputs, numOutputs, true);

            if (gGlobal->gVectorSwitch) {
                comp = new DAGInstructionsCompiler(container);
            } else {
                comp = new InstructionsCompiler(container);
            }

            comp->compileMultiSignal(signals);
            container->dump(dst);
            throw faustexception("");
        }
        if (!container) {
            stringstream error;
            error << "ERROR : cannot find compiler for " << "\"" << gOutputLang  << "\"" << endl;
            throw faustexception(error.str());
        }
        if (gGlobal->gVectorSwitch) {
            comp = new DAGInstructionsCompiler(container);
        } else {
            comp = new InstructionsCompiler(container);
        }

        if (gPrintXMLSwitch) comp->setDescription(new Description());
        if (gPrintDocSwitch) comp->setDescription(new Description());

        comp->compileMultiSignal(signals);

        /****************************************************************
         * generate output file
         ****************************************************************/
        ifstream* enrobage;
         
        if (gArchFile) {
        
            // Keep current directory
            char current_directory[FAUST_PATH_MAX];
            getcwd(current_directory, FAUST_PATH_MAX);
            
            if ((enrobage = open_arch_stream(gArchFile))) {
                
                if (strcmp(gOutputLang, "js") != 0) {
                    printheader(*dst);
                }
                
                if ((strcmp(gOutputLang, "c") == 0) || (strcmp(gOutputLang, "cpp") == 0)) {
                    tab(0, *dst); *dst << "#ifndef  __" << gGlobal->gClassName << "_H__";
                    tab(0, *dst); *dst << "#define  __" << gGlobal->gClassName << "_H__" << std::endl;
                }

                streamCopyUntil(*enrobage, *dst, "<<includeIntrinsic>>");
                streamCopyUntil(*enrobage, *dst, "<<includeclass>>");

                if (gGlobal->gOpenCLSwitch || gGlobal->gCUDASwitch) {
                    istream* thread_include = open_arch_stream("thread.h");
                    if (thread_include) {
                        streamCopy(*thread_include, *dst);
                    }
                    delete(thread_include);
                }

                if ((strcmp(gOutputLang, "java") != 0) && (strcmp(gOutputLang, "js") != 0)) {
                    printfloatdef(*dst, (gGlobal->gFloatSize == 3));
                }

                if (strcmp(gOutputLang, "c") == 0) {
                    *dst << "#include <stdlib.h>"<< std::endl;
                }

                container->produceClass();
                streamCopyUntilEnd(*enrobage, *dst);
                if (gGlobal->gSchedulerSwitch) {
                    istream* scheduler_include = open_arch_stream("scheduler.cpp");
                    if (scheduler_include) {
                        streamCopy(*scheduler_include, *dst);
                    }
                     delete(scheduler_include);
                }

                if ((strcmp(gOutputLang, "c") == 0) || (strcmp(gOutputLang, "cpp") == 0)) {
                    tab(0, *dst); *dst << "#endif"<< std::endl;
                }
                
                // Restore current_directory
                chdir(current_directory);
                delete(enrobage);
                 
            } else {
                stringstream error;
                error << "ERROR : can't open architecture file " << gArchFile << endl;
                throw faustexception(error.str());
            }
            
        } else {
            if (strcmp(gOutputLang, "js") != 0) {
                printheader(*dst);
            }
            if ((strcmp(gOutputLang, "java") != 0) && (strcmp(gOutputLang, "js") != 0)) {
                printfloatdef(*dst, (gGlobal->gFloatSize == 3));
            }
            if (strcmp(gOutputLang, "c") == 0) {
                *dst << "#include <stdlib.h>"<< std::endl;
            }
            container->produceClass();
        }
    }
    
    endTiming("compilation");

    return make_pair(comp, container);
}

static void generateOutputFiles(InstructionsCompiler * comp, CodeContainer * container)
{
    /****************************************************************
     1 - generate XML description (if required)
    *****************************************************************/
  
    if (gPrintXMLSwitch) {
        Description* D = comp->getDescription(); assert(D);
        ofstream xout(subst("$0.xml", makeDrawPath()).c_str());
      
        if (gGlobal->gMetaDataSet.count(tree("name")) > 0)          D->name(tree2str(*(gGlobal->gMetaDataSet[tree("name")].begin())));
        if (gGlobal->gMetaDataSet.count(tree("author")) > 0)        D->author(tree2str(*(gGlobal->gMetaDataSet[tree("author")].begin())));
        if (gGlobal->gMetaDataSet.count(tree("copyright")) > 0)     D->copyright(tree2str(*(gGlobal->gMetaDataSet[tree("copyright")].begin())));
        if (gGlobal->gMetaDataSet.count(tree("license")) > 0)       D->license(tree2str(*(gGlobal->gMetaDataSet[tree("license")].begin())));
        if (gGlobal->gMetaDataSet.count(tree("version")) > 0)       D->version(tree2str(*(gGlobal->gMetaDataSet[tree("version")].begin())));

        D->className(gGlobal->gClassName);
		D->inputs(container->inputs());
		D->outputs(container->outputs());

        D->print(0, xout);
    }

    /****************************************************************
     2 - generate documentation from Faust comments (if required)
    *****************************************************************/

    if (gPrintDocSwitch) {
        if (gGlobal->gLatexDocSwitch) {
            printDoc(subst("$0-mdoc", makeDrawPathNoExt()).c_str(), "tex", FAUSTVERSION);
        }
    }

    /****************************************************************
     3 - generate the task graph file in dot format
    *****************************************************************/

    if (gGraphSwitch) {
        ofstream dotfile(subst("$0.dot", makeDrawPath()).c_str());
        container->printGraphDotFormat(dotfile);
    }
}

static string expand_dsp_internal(int argc, const char* argv[], const char* name, const char* input = NULL)
{

    /****************************************************************
     1 - process command line
    *****************************************************************/
    process_cmdline(argc, argv);
   
    /****************************************************************
     2 - parse source files
    *****************************************************************/
    if (input) {
        gGlobal->gInputString = input;
        gGlobal->gInputFiles.push_back(name);
    }
    parseSourceFiles();
    
    initFaustDirectories();

    /****************************************************************
     3 - evaluate 'process' definition
    *****************************************************************/
    
    // int numInputs, numOutputs;
    // Tree process = evaluateBlockDiagram(gGlobal->gExpandedDefList, numInputs, numOutputs);
    
    call_fun(thread_evaluateBlockDiagram); // In a thread with more stack size...
    if (!gProcessTree) {
        throw faustexception(gErrorMessage);
    }
    stringstream out;
    out << "process = " << boxpp(gProcessTree) << ";" << endl;
    return out.str();
}

void compile_faust_internal(int argc, const char* argv[], const char* name, const char* input = NULL)
{
    gHelpSwitch = false;
    gVersionSwitch = false;
    gGraphSwitch = false;
    gDrawPSSwitch = false;
    gDrawSVGSwitch = false;
    gPrintXMLSwitch = false;
    gPrintDocSwitch = false;
    gBalancedSwitch = 0;
    gArchFile = 0;

    gTimeout = INT_MAX;           
    gPrintFileListSwitch = false;
    gOutputLang = 0;
  
    /****************************************************************
     1 - process command line
    *****************************************************************/
    process_cmdline(argc, argv);
    
    if (gHelpSwitch) { 
        printhelp(); 
        throw faustexception("");
    }
    if (gVersionSwitch) { 
        printversion(); 
        throw faustexception(""); 
    }

#ifndef WIN32
    alarm(gTimeout);
#endif

    /****************************************************************
     2 - parse source files
    *****************************************************************/
    if (input) {
        gGlobal->gInputString = input;
        gGlobal->gInputFiles.push_back(name);
    }
    parseSourceFiles();
    
    initFaustDirectories();

    /****************************************************************
     3 - evaluate 'process' definition
    *****************************************************************/
    
    // int numInputs, numOutputs;
    // Tree process = evaluateBlockDiagram(gGlobal->gExpandedDefList, numInputs, numOutputs);
    
    call_fun(thread_evaluateBlockDiagram); // In a thread with more stack size...
    if (!gProcessTree) {
        throw faustexception(gErrorMessage);
    }
    Tree process = gProcessTree;
    int numInputs = gNumInputs;
    int numOutputs = gNumOutputs;
    
    if (gExportDSP) {
        ofstream xout(subst("$0-exp.dsp", makeDrawPathNoExt()).c_str());
        xout << "process = " << boxpp(process) << ";" << endl;
        return;
    }

    /****************************************************************
     4 - compute output signals of 'process'
    *****************************************************************/
    startTiming("propagation");

    //Tree lsignals = boxPropagateSig(gGlobal->nil, process, makeSigInputList(numInputs));
    
    call_fun(thread_boxPropagateSig); // In a thread with more stack size...
    if (!gLsignalsTree) {
        throw faustexception(gErrorMessage);
    }
    Tree lsignals = gLsignalsTree;
 
    if (gGlobal->gDetailsSwitch) {
        cout << "output signals are : " << endl;
        Tree ls = lsignals;
        while (! isNil(ls)) {
            cout << ppsig(hd(ls)) << endl;
            ls = tl(ls);
        }
    }

    endTiming("propagation");

    /****************************************************************
    5 - preparation of the signal tree and translate output signals into C, C++, JAVA, JavaScript or LLVM code
    *****************************************************************/
    pair<InstructionsCompiler*, CodeContainer*> comp_container = generateCode(lsignals, numInputs, numOutputs);

    /****************************************************************
     6 - generate xml description, documentation or dot files
    *****************************************************************/
    generateOutputFiles(comp_container.first, comp_container.second);
}

EXPORT LLVMResult* compile_faust_llvm(int argc, const char* argv[], const char* name, const char* input, char* error_msg)
{
    gLLVMOut = false;
    gGlobal = NULL;
    LLVMResult* res;
    
    try {
        global::allocate();
        compile_faust_internal(argc, argv, name, input);
        strncpy(error_msg, gGlobal->gErrorMsg.c_str(), 256);  
        res = gGlobal->gLLVMResult;
    } catch (faustexception& e) {
        strncpy(error_msg, e.Message().c_str(), 256);
        res = NULL;
    }
    
    global::destroy();
    return res;
}

EXPORT int compile_faust(int argc, const char* argv[], const char* name, const char* input, char* error_msg)
{
    gLLVMOut = true;
    gGlobal = NULL;
    int res;
    
    try {
        global::allocate();     
        compile_faust_internal(argc, argv, name, input);
        strncpy(error_msg, gGlobal->gErrorMsg.c_str(), 256);
        res = 0;
    } catch (faustexception& e) {
        strncpy(error_msg, e.Message().c_str(), 256);
        res = -1;
    }
    
    global::destroy();
    return res;
}

EXPORT string expand_dsp(int argc, const char* argv[], const char* name, const char* input, char* error_msg)
{
    gGlobal = NULL;
    string res = "";
    
    try {
        global::allocate();       
        res = expand_dsp_internal(argc, argv, name, input);
        strncpy(error_msg, gGlobal->gErrorMsg.c_str(), 256);
    } catch (faustexception& e) {
        strncpy(error_msg, e.Message().c_str(), 256);
    }
    
    global::destroy();
    return res;
}

