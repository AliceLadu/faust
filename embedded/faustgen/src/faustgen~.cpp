/************************************************************************
    FAUST Architecture File
    Copyright (C) 2010-2013 GRAME, Centre National de Creation Musicale
    ---------------------------------------------------------------------
    This Architecture section is free software; you can redistribute it
    and/or modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 3 of
    the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; If not, see <http://www.gnu.org/licenses/>.

    EXCEPTION : As a special exception, you may create a larger work
    that contains this FAUST architecture section and distribute
    that work under terms of your choice, so long as this FAUST
    architecture section is not modified.


 ************************************************************************
 ************************************************************************/

#include "faustgen~.h"
#include "base64.h"

int faustgen_factory::gFaustCounter = 0;
map<string, faustgen_factory*> faustgen_factory::gFactoryMap;

//===================
// Faust DSP Factory
//===================

#ifdef __APPLE__
static string getTarget()
{
    int tmp;
    return (sizeof(&tmp) == 8) ? "" : "i386-apple-darwin10.6.0";
}
#else
static string getTarget() { return ""; }
#endif

struct Max_Meta : public Meta
{
    void declare(const char* key, const char* value)
    {   
        if ((strcmp("name", key) == 0) || (strcmp("author", key) == 0)) {
            post("%s : %s", key, value);
        }
    }
};

faustgen_factory::faustgen_factory(const string& name)
{
    m_siginlets = 0;
    m_sigoutlets = 0;
    
    fUpdateInstance = 0;
    fName = name;
    fDSPfactory = 0;
    fBitCodeSize = 0;
    fBitCode = 0;
    fSourceCodeSize = 0;
    fSourceCode = 0;
    gFaustCounter++;
    fFaustNumber = gFaustCounter;
    
#ifdef __APPLE__
    // OSX only : access to the fautgen~ bundle
    CFBundleRef faustgen_bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.grame.faustgen"));
    CFURLRef faustgen_ref = CFBundleCopyBundleURL(faustgen_bundle);
    UInt8 bundle_path[512];
    Boolean res = CFURLGetFileSystemRepresentation(faustgen_ref, true, bundle_path, 512);
    assert(res);
    
    // Built the complete resource path
    fLibraryPath = string((const char*)bundle_path) + string(FAUST_LIBRARY_PATH);

	// Draw path in temporary folder
    fDrawPath = string(FAUST_DRAW_PATH);
#endif

#ifdef WIN32
	HMODULE handle = LoadLibrary("faustgen~.mxe");
	if (handle) {
		// Get faustgen~.mxe path
		char name[512];
		GetModuleFileName(handle, name, 512);
		string str_name = string(name);
		str_name = str_name.substr(0, str_name.find_last_of("\\"));
		// Built the complete resource path
		fLibraryPath = string(str_name) + string(FAUST_LIBRARY_PATH);
		// Draw path in temporary folder
        TCHAR lpTempPathBuffer[MAX_PATH];
        // Gets the temp path env string (no guarantee it's a valid path).
        DWORD dwRetVal = GetTempPath(MAX_PATH, lpTempPathBuffer); 
        if (dwRetVal > MAX_PATH || (dwRetVal == 0)) {
            post("GetTempPath failed...");
            // Try our value instead...  
            fDrawPath = string(str_name) + string(FAUST_DRAW_PATH);
        } else {
            fDrawPath = string(lpTempPathBuffer);
        }
		FreeLibrary(handle);
	} else {
		post("Error : cannot locate faustgen~.mxe...");
		fLibraryPath = "";
		fDrawPath = "";
	}
 #endif

    t_max_err err = systhread_mutex_new(&fDSPMutex, SYSTHREAD_MUTEX_NORMAL);
    if (err != MAX_ERR_NONE) {
        post("Cannot allocate mutex...");
    }
}
        
faustgen_factory::~faustgen_factory()
{
    free_dsp_factory();
    free_sourcecode();
    free_bitcode();
   
    remove_svg();
    systhread_mutex_free(fDSPMutex);
}

void faustgen_factory::free_sourcecode()
{
    if (fSourceCode) {
        sysmem_freehandle(fSourceCode);
        fSourceCodeSize = 0;
        fSourceCode = 0;
    }
}

void faustgen_factory::free_bitcode()
{
    if (fBitCode) {
        sysmem_freehandle(fBitCode);
        fBitCodeSize = 0;
        fBitCode = 0;
    }
}

void faustgen_factory::free_dsp_factory()
{
   if (lock()) {
   
        // Free all instances
        set<faustgen*>::const_iterator it;
        for (it = fInstances.begin(); it != fInstances.end(); it++) {
            (*it)->free_dsp();
        }
     
        deleteDSPFactory(fDSPfactory);
        fDSPfactory = 0;
        unlock();
    } else {
        post("Mutex lock cannot be taken...");
    }
}

llvm_dsp_factory* faustgen_factory::create_factory_from_bitcode()
{
    string decoded_bitcode = base64_decode(*fBitCode, fBitCodeSize);
    return readDSPFactoryFromBitcode(decoded_bitcode, getTarget());
    /*
    // Alternate model using LLVM IR
    return readDSPFactoryFromIR(*fBitCode, getTarget());
    */
}

llvm_dsp_factory* faustgen_factory::create_factory_from_sourcecode(faustgen* instance)
{
    char name_app[64];
    sprintf(name_app, "faustgen-%d", fFaustNumber);
    
    // To be sure we get a correct SVG diagram...
    remove_svg();
    print_compile_options();
    
    // Prepare compile options
    std::string error;
 	const char* argv[32];
    
    if (find(fCompileOptions.begin(), fCompileOptions.end(), fLibraryPath) == fCompileOptions.end()) {
        fCompileOptions.push_back("-I");
        fCompileOptions.push_back(fLibraryPath);
    }
    if (find(fCompileOptions.begin(), fCompileOptions.end(), fDrawPath) == fCompileOptions.end()) {
        fCompileOptions.push_back("-O");
        fCompileOptions.push_back(fDrawPath);
    }
    
	assert(fCompileOptions.size() < 32);
    CompileOptionsIt it;
    int i = 0;
    for (it = fCompileOptions.begin(); it != fCompileOptions.end(); it++, i++) {
        argv[i] = (char*)(*it).c_str();
    }
    
    llvm_dsp_factory* factory = createDSPFactoryFromString(name_app, *fSourceCode, fCompileOptions.size(), argv, getTarget(), error, LLVM_OPTIMIZATION);
    
    if (factory) {
        return factory;
    } else {
        if (fUpdateInstance == instance) {
            instance->hilight_on(error);
        }
		post("Invalid Faust code or compile options : %s", error.c_str());
        return 0;
    }
}

llvm_dsp* faustgen_factory::create_dsp_default(int ins, int outs)
{
    return new default_llvm_dsp(ins, outs);
}

llvm_dsp* faustgen_factory::create_dsp_aux(faustgen* instance)
{
    llvm_dsp* dsp = 0;
    Max_Meta meta;
    
    // Factory already allocated
    if (fDSPfactory) {
        dsp = createDSPInstance(fDSPfactory);
        post("Factory already allocated, %i input(s), %i output(s)", dsp->getNumInputs(), dsp->getNumOutputs());
        goto end;
    }
   
    // Tries to create from bitcode
    if (fBitCodeSize > 0) {
        fDSPfactory = create_factory_from_bitcode();
        if (fDSPfactory) {
            metadataDSPFactory(fDSPfactory, &meta);
            dsp = createDSPInstance(fDSPfactory);
            post("Compilation from bitcode succeeded, %i input(s), %i output(s)", dsp->getNumInputs(), dsp->getNumOutputs());
            goto end; 
        }
    }

    // Otherwise tries to create from source code
    if (fSourceCodeSize > 0) {
        fDSPfactory = create_factory_from_sourcecode(instance);
        if (fDSPfactory) {
            metadataDSPFactory(fDSPfactory, &meta);
            dsp = createDSPInstance(fDSPfactory);
            post("Compilation from source code succeeded, %i input(s), %i output(s)", dsp->getNumInputs(), dsp->getNumOutputs());
            goto end; 
        } 
    }

    // Otherwise creates default DSP keeping the same input/output number
    dsp = create_dsp_default(m_siginlets, m_sigoutlets);
    post("Allocation of default DSP succeeded, %i input(s), %i output(s)", dsp->getNumInputs(), dsp->getNumOutputs());
 
 end:
    
    assert(dsp);
    m_siginlets = dsp->getNumInputs();
    m_sigoutlets = dsp->getNumOutputs();
    return dsp;
}

void faustgen_factory::print_compile_options()
{
    if (fCompileOptions.size() > 0) {
        post("-----------------------------");
        CompileOptionsIt it;
        for (it = fCompileOptions.begin(); it != fCompileOptions.end(); it++) {
            post("Compile option = %s", (*it).c_str());
        }
        post("-----------------------------");
    }
}

void faustgen_factory::default_compile_options()
{
    // Clear and set default value
    fCompileOptions.clear();
    
    if (sizeof(FAUSTFLOAT) == 8) {
        fCompileOptions.push_back("-double");
    }
    
    // Add -svg to current compile options
    fCompileOptions.push_back("-svg");

    // Vector mode by default
    /*
    fCompileOptions.push_back("-vec");
    fCompileOptions.push_back("-lv");
    fCompileOptions.push_back("1");
    */
    /*
    Seems not necessary...
    fCompileOptions.push_back("-vs");
    stringstream num;
    num << sys_getblksize();
    fCompileOptions.push_back(num.str());
    */
}

void faustgen_factory::getfromdictionary(t_dictionary* d)
{
    t_max_err err;
    
    // Read sourcecode "faustgen_version" key
    const char* faustgen_version;  
    err = dictionary_getstring(d, gensym("version"), &faustgen_version);  
      
    if (err != MAX_ERR_NONE) {
        post("Cannot read \"version\" key, so ignore bitcode, force recompilation and use default compileoptions");
        // Use default option
        default_compile_options();
        goto read_sourcecode;
    } else if (strcmp(faustgen_version, FAUSTGEN_VERSION) != 0) {
        post("Older version of faustgen~ (%s versus %s), so ignore bitcode, force recompilation and use default compileoptions", FAUSTGEN_VERSION, faustgen_version);
        // Use default option
        default_compile_options();
        goto read_sourcecode;
    }
    
    // Read sourcecode "compileoptions" key
    long argc;
    t_atom* argv;
    err = dictionary_getatoms(d, gensym("compile_options"), &argc, &argv);
    if (err == MAX_ERR_NONE) {
        t_atom* ap = argv;
        for (int i = 0; i < argc; i++, ap++) {
			fCompileOptions.push_back(atom_getsym(ap)->s_name);
        }
    } else {
        // If not found, use default option
        default_compile_options();
    }
    
    // Read bitcode size key
    err = dictionary_getlong(d, gensym("bitcode_size"), (t_atom_long*)&fBitCodeSize); 
    if (err != MAX_ERR_NONE) {
        fBitCodeSize = 0;
        goto read_sourcecode;
    }
    
    // If OK read bitcode
    fBitCode = sysmem_newhandleclear(fBitCodeSize + 1);             // We need to use a size larger by one for the null terminator
    const char* bitcode;
    err = dictionary_getstring(d, gensym("bitcode"), &bitcode);     // The retrieved pointer references the string in the dictionary, it is not a copy.
    sysmem_copyptr(bitcode, *fBitCode, fBitCodeSize);
    if (err != MAX_ERR_NONE) {
        fBitCodeSize = 0;
    }

read_sourcecode:    

    // Read sourcecode size key
    err = dictionary_getlong(d, gensym("sourcecode_size"), (t_atom_long*)&fSourceCodeSize); 
    if (err != MAX_ERR_NONE) {
        goto default_sourcecode;
    }
    
    // If OK read sourcecode 
    fSourceCode = sysmem_newhandleclear(fSourceCodeSize + 1);           // We need to use a size larger by one for the null terminator
    const char* sourcecode;
    err = dictionary_getstring(d, gensym("sourcecode"), &sourcecode);   // The retrieved pointer references the string in the dictionary, it is not a copy.
    sysmem_copyptr(sourcecode, *fSourceCode, fSourceCodeSize);
    if (err == MAX_ERR_NONE) {
        return;
    }
    
default_sourcecode:

    // Otherwise tries to create from default source code
    fSourceCodeSize = strlen(DEFAULT_SOURCE_CODE);
    fSourceCode = sysmem_newhandleclear(fSourceCodeSize + 1);
    sysmem_copyptr(DEFAULT_SOURCE_CODE, *fSourceCode, fSourceCodeSize);
}

// Called when saving the Max patcher
// This function saves the necessary data inside the json file (Faust sourcecode)
void faustgen_factory::appendtodictionary(t_dictionary* d)
{
    post("Saving object version, compiler options, sourcecode and bitcode...");
    
    // Save faustgen~ version
    dictionary_appendstring(d, gensym("version"), FAUSTGEN_VERSION);
    
    // Save compile options
    t_atom* compileoptions = 0;
    long ac;
    char res;
    
    print_compile_options();
    atom_alloc_array(fCompileOptions.size(), &ac, &compileoptions, &res);
    
    if (res) {
        CompileOptionsIt it;
        t_atom*ap = compileoptions;
        for (it = fCompileOptions.begin(); it != fCompileOptions.end(); it++, ap++) {
            atom_setsym(ap, gensym((char*)(*it).c_str()));
        }
        dictionary_chuckentry(d, gensym("compile_options"));
        dictionary_appendatoms(d, gensym("compile_options"), fCompileOptions.size(), compileoptions);
    } else {
        post("Cannot allocate atom array...");
    }
    
    // Save source code
    if (fSourceCodeSize) {
        dictionary_appendlong(d, gensym("sourcecode_size"), fSourceCodeSize);
        dictionary_appendstring(d, gensym("sourcecode"), *fSourceCode);
    }
      
    // Save bitcode
    if (fDSPfactory) {
        string bitcode = writeDSPFactoryToBitcode(fDSPfactory);
        std::string encoded_bitcode = base64_encode((const unsigned char*)bitcode.c_str(), bitcode.size());
        dictionary_appendlong(d, gensym("bitcode_size"), encoded_bitcode.size());
        dictionary_appendstring(d, gensym("bitcode"), encoded_bitcode.c_str());  
    }
    /*
    // Alternate model using LLVM IR
    string ircode = writeDSPFactoryToIR(fDSPfactory);
    dictionary_appendlong(d, gensym("bitcode_size"), ircode.size());
    dictionary_appendstring(d, gensym("bitcode"), ircode.c_str()); 
    */ 
}

bool faustgen_factory::try_open_svg()
{
    // Open the svg diagram file inside a web browser
    char command[512];
#ifdef WIN32
	sprintf(command, "type \"file:///%sfaustgen-%d-svg/process.svg\"", fDrawPath.c_str(), fFaustNumber);
#else
	sprintf(command, "open -a Safari \"file://%sfaustgen-%d-svg/process.svg\"", fDrawPath.c_str(), fFaustNumber);
#endif
	return (system(command) == 0);
}

void faustgen_factory::open_svg()
{
    // Open the svg diagram file inside a web browser
    char command[512];
#ifdef WIN32
	sprintf(command, "start \"\" \"file:///%sfaustgen-%d-svg/process.svg\"", fDrawPath.c_str(), fFaustNumber);
#else
	sprintf(command, "open -a Safari \"file://%sfaustgen-%d-svg/process.svg\"", fDrawPath.c_str(), fFaustNumber);
#endif
	system(command);
}

void faustgen_factory::remove_svg()
{
    // Possibly done by "compileoptions" or display_svg
    char command[512];
#ifdef WIN32
    sprintf(command, "rmdir /S/Q \"%sfaustgen-%d-svg\"", fDrawPath.c_str(), fFaustNumber);
#else
    sprintf(command, "rm -r \"%sfaustgen-%d-svg\"", fDrawPath.c_str(), fFaustNumber);
#endif
    system(command); 
}

void faustgen_factory::display_svg()
{
    // Try to open SVG svg diagram file inside a web browser
    if (!try_open_svg()) {
    
        post("SVG diagram not available, recompile to produce it");
        
        // Force recompilation to produce it
        llvm_dsp_factory* factory = create_factory_from_sourcecode(0);
        deleteDSPFactory(factory);
     
        // Open the SVG diagram file inside a web browser
        open_svg();
    }
}

bool faustgen_factory::open_file(const char* file)
{
    char command[512];
#ifdef WIN32
	sprintf(command, "start \"\" \"%s%s\"", fLibraryPath.c_str(), file);
#else
	sprintf(command, "open \"%s%s\"", fLibraryPath.c_str(), file);
#endif
	post(command);
    return (system(command) == 0);
}

bool faustgen_factory::open_file(const char* appl, const char* file)
{
    char command[512];
#ifdef WIN32
  	sprintf(command, "start \"\" %s \"%s%s\"", appl, fLibraryPath.c_str(), file);	
#else
	sprintf(command, "open -a %s \"%s%s\"", appl, fLibraryPath.c_str(), file);
#endif
    return (system(command) == 0);
}

void faustgen_factory::display_pdf()
{
    // Open the PDF documentation
    open_file(FAUST_PDF_DOCUMENTATION);
}

void faustgen_factory::display_libraries_aux(const char* lib)
{
    const char* appl;
    int i = 0;
    
    while ((appl = TEXT_APPL_LIST[i++]) && (strcmp(appl, "") != 0)) {
        if (open_file(appl, lib)) {
            break;
        }
    }
}

void faustgen_factory::display_libraries()
{
	// Open the libraries
#ifdef WIN32
	open_file("effect.lib");
    open_file("filter.lib");
    open_file("math.lib");
    open_file("maxmsp.lib");
    open_file("music.lib");
    open_file("oscillator.lib");
    open_file("reduce.lib");
#else
    display_libraries_aux("effect.lib");
    display_libraries_aux("filter.lib");
    display_libraries_aux("math.lib");
    display_libraries_aux("maxmsp.lib");
    display_libraries_aux("music.lib");
    display_libraries_aux("oscillator.lib");
    display_libraries_aux("reduce.lib");
#endif
}

void faustgen_factory::update_sourcecode(int size, char* source_code, faustgen* instance)
{
    // Recompile only if text has been changed
    if (strcmp(source_code, *fSourceCode) != 0) {
        
        // Update all instances
        set<faustgen*>::const_iterator it;
        for (it = fInstances.begin(); it != fInstances.end(); it++) {
            (*it)->hilight_off();
        }

        // Delete the existing Faust module
        free_dsp_factory();

        // Free the memory allocated for fSourceCode
        free_sourcecode();
    
        // Free the memory allocated for fBitCode
        free_bitcode();
    
        // Allocate the right memory for fSourceCode
        fSourceCode = sysmem_newhandleclear(size + 1);
        sysmem_copyptr(source_code, *fSourceCode, size);
        fSourceCodeSize = size;
         
        // Update all instances
        fUpdateInstance = instance;
        for (it = fInstances.begin(); it != fInstances.end(); it++) {
            (*it)->update_sourcecode();
        }
    } else {
        post("DSP code has not been changed...");
    }
}

void faustgen_factory::read(long inlet, t_symbol* s)
{
    char filename[MAX_FILENAME_CHARS];
    short path = 0;
    long type = 'TEXT';
    short err;
    t_filehandle fh;
    
    // No filename, so open load dialog
    if (s == gensym("")) {
        filename[0] = 0;
        if (open_dialog(filename, &path, (t_fourcc*)&type, (t_fourcc*)&type, 1)) {
            post("Faust DSP file not found");
            return;
        }
    // Otherwise locate the file
    } else {
        strcpy(filename, s->s_name);
        if (locatefile_extended(filename, &path, (t_fourcc*)&type, (t_fourcc*)&type, 1)) {
            post("Faust DSP file '%s' not found", filename);
            return;
        }
    }
    
    // File found, open it and recompile DSP
    err = path_opensysfile(filename, path, &fh, READ_PERM);
    if (err) {
        post("Faust DSP file '%s' cannot be opened", filename);
        return;
    }
    
    // Delete the existing Faust module
    free_dsp_factory();

    // Free the memory allocated for fBitCode
    free_bitcode();

    err = sysfile_readtextfile(fh, fSourceCode, 0, (t_sysfile_text_flags)(TEXT_LB_UNIX | TEXT_NULL_TERMINATE));
    if (err) {
        post("Faust DSP file '%s' cannot be read", filename);
    }
    
    sysfile_close(fh);
    fSourceCodeSize = sysmem_handlesize(fSourceCode);
    
    // Add DSP file enclosing folder pathname in the '-I' list
    char full_path[MAX_FILENAME_CHARS];
    if (path_topathname(path, filename, full_path) == 0) {
        string full_path_str = full_path;
        size_t first = full_path_str.find_first_of(SEPARATOR);
        size_t last = full_path_str.find_last_of(SEPARATOR);
        string folder_path = (first != string::npos && last != string::npos) ? full_path_str.substr(first, last - first) : "";
        if ((folder_path != "") && find(fCompileOptions.begin(), fCompileOptions.end(), folder_path) == fCompileOptions.end() ) {
            fCompileOptions.push_back("-I");
            fCompileOptions.push_back(folder_path);
        }
    }
    
    // Update all instances
    set<faustgen*>::const_iterator it;
    for (it = fInstances.begin(); it != fInstances.end(); it++) {
        (*it)->update_sourcecode();
    }
}

void faustgen_factory::write(long inlet, t_symbol* s)
{
    char filename[MAX_FILENAME_CHARS];
    short path = 0;
    long type = 'TEXT';
    short err;
    t_filehandle fh;
    
    // No filename, so open save dialog
    if (s == gensym("")) {
        filename[0] = 0;
        if (saveas_dialog(filename, &path, NULL)) {
            post("Faust DSP file not found");
            return;
        } else {
            err = path_createsysfile(filename, path, type, &fh);
            if (err) {
                post("Faust DSP file '%s' cannot be created", filename);
                return;
            }
        }
    // Otherwise locate or create the file
    } else {
        strcpy(filename, s->s_name);
        if (locatefile_extended(filename, &path, (t_fourcc*)&type, (t_fourcc*)&type, 1)) {
            post("Faust DSP file '%s' not found, so tries to create it", filename);
            err = path_createsysfile(filename, path, type, &fh);
            if (err) {
                post("Faust DSP file '%s' cannot be created", filename);
                return;
            }
        } else {
            err = path_opensysfile(filename, path, &fh, WRITE_PERM);
            if (err) {
                post("Faust DSP file '%s' cannot be opened", filename);
                return;
            }
        }
    }
    
    err = sysfile_writetextfile(fh, fSourceCode, (t_sysfile_text_flags)(TEXT_LB_UNIX | TEXT_NULL_TERMINATE));
    if (err) {
        post("Faust DSP file '%s' cannot be written", filename);
    }
    sysfile_close(fh);
}

void faustgen_factory::compileoptions(long inlet, t_symbol* s, long argc, t_atom* argv) 
{ 
    post("Compiler options modified for faustgen");
    
    if (argc == 0) {
        post("No argument entered, no additional compilation option will be used");
    }
    
    int i;
    t_atom* ap;
    
    // First reset compiler options
    default_compile_options();
    
    bool optimize = false;
   
    // Increment ap each time to get to the next atom
    for (i = 0, ap = argv; i < argc; i++, ap++) {
        switch (atom_gettype(ap)) {
            case A_LONG: {
                std::stringstream num;
				num << atom_getlong(ap);
				string res = num.str();
				fCompileOptions.push_back(res.c_str());
				break;
            }
            case A_FLOAT:
                post("Invalid compiler option argument - float");
                break;
            case A_SYM:
                // Add options to default ones
                if (strcmp("-opt", atom_getsym(ap)->s_name) == 0) {
                    optimize = true;
                } else {
                    fCompileOptions.push_back(atom_getsym(ap)->s_name);
                }
				break;
            default:
                post("Invalid compiler option argument - unknown");
                break;
        }
    }
    
    if (optimize) {
 
        post("Start looking for optimal compilation options...");
        
	#ifndef WIN32
        FaustLLVMOptimizer optimizer(string(*fSourceCode), fLibraryPath, getTarget(), 2000, sys_getblksize());
        fCompileOptions = optimizer.findOptimize();
	#endif
        
        if (sizeof(FAUSTFLOAT) == 8) {
            fCompileOptions.push_back("-double");
        }
    
        // Add -svg to current compile options
        fCompileOptions.push_back("-svg");
   
        post("Optimal compilation options found");
    }
    
    // Delete the existing Faust module
    free_dsp_factory();
    
    // Free the memory allocated for fBitCode
    free_bitcode();
     
    // Update all instances
    set<faustgen*>::const_iterator it;
    for (it = fInstances.begin(); it != fInstances.end(); it++) {
        (*it)->update_sourcecode();
    }
}

//====================
// Faust DSP Instance
//====================

bool faustgen::allocate_factory(const string& effect_name)
{
    bool res = false;
     
    if (faustgen_factory::gFactoryMap.find(effect_name) != faustgen_factory::gFactoryMap.end()) {
        fDSPfactory = faustgen_factory::gFactoryMap[effect_name];
    } else {
        fDSPfactory = new faustgen_factory(effect_name);
        faustgen_factory::gFactoryMap[effect_name] = fDSPfactory;
        res = true;
    }
    
    fDSPfactory->add_instance(this);
    return res;
}

faustgen::faustgen(t_symbol* sym, long ac, t_atom* argv) 
{ 
    m_siginlets = 0;
    m_sigoutlets = 0;
    

    fDSP = 0;
    fDSPfactory = 0;
    fEditor = 0;
    fMute = false;
    
    int i;
    t_atom* ap;
    bool res = false;
    
    // Allocate factory with a given "name"
    for (i = 0, ap = argv; i < ac; i++, ap++) {
        if (atom_gettype(ap) == A_SYM) {
            res = allocate_factory(atom_getsym(ap)->s_name);
            break;
        }
    }
    
    // Empty (= no name) faustgen~ will be internally separated as groups with different names
    if (!fDSPfactory) {
        string effect_name;
        stringstream num;
        num << faustgen_factory::gFaustCounter;
        effect_name = "faustgen_factory-" + num.str();
        res = allocate_factory(effect_name);
    }
     
    t_object* box;
    object_obex_lookup((t_object*)&m_ob, gensym("#B"), &box);
    jbox_get_color(box, &fDefaultColor);
    
    // Needed to script objects
    char name[64];
    sprintf(name, "faustgen-%x", this);
    jbox_set_varname(box, gensym(name));
     
    // Fetch the data inside the max patcher using the dictionary
    t_dictionary* d = 0;
    
    if ((d = (t_dictionary*)gensym("#D")->s_thing) && res) {
        fDSPfactory->getfromdictionary(d);
    }
        
    create_dsp(true);
}

// Called upon deleting the object inside the patcher
faustgen::~faustgen() 
{ 
     free_dsp();
     
    if (fEditor) {
        object_free(fEditor);
        fEditor = 0;
    }
     
    fDSPfactory->remove_instance(this);
}

void faustgen::free_dsp()
{
    deleteDSPInstance(fDSP);
    fDSPUI.clear();
    fDSP = 0;
}

static bool check_digit(const string& name)
{
    for (int i = name.size() - 1; i >= 0; i--) {
        if (isdigit(name[i])) return true;
    }
    return false;
}

static int count_digit(const string& name)
{
    int count = 0;
    for (int i = name.size() - 1; i >= 0; i--) {
        if (isdigit(name[i])) count++;
    }
    return count;
}

// Called upon sending the object a message inside the max patcher
// Allows to set a value to the Faust module's parameter, or a list of values
void faustgen::anything(long inlet, t_symbol* s, long ac, t_atom* av)
{
    bool res = false;
    
    if (ac < 0) return;
    
    // Check if no argument is there, consider it is a toggle message for a button
    if (ac == 0) {
        
        string name = string((s)->s_name);
        float off = 0.0f;
        float on = 1.0f;
        fDSPUI.setValue(name, off);
        fDSPUI.setValue(name, on);
        
        av[0].a_type = A_FLOAT;
        av[0].a_w.w_float = off;
        anything(inlet, s, 1, av);
        
        return;
    }

    string name = string((s)->s_name);
    
    // List of values
    if (check_digit(name)) {
        
        int ndigit = 0;
        int pos;
        
        for (pos = name.size() - 1; pos >= 0; pos--) {
            if (isdigit(name[pos]) || name[pos] == ' ') {
                ndigit++;
            } else {
                break;
            }
        }
        pos++;
        
        string prefix = name.substr(0, pos);
        string num_base = name.substr(pos);
        int num = atoi(num_base.c_str());
        
        int i;
        t_atom* ap;
       
        // Increment ap each time to get to the next atom
        for (i = 0, ap = av; i < ac; i++, ap++) {
            float value;
            switch (atom_gettype(ap)) {
                case A_LONG: {
                    value = (float)ap[0].a_w.w_long;
                    break;
                }
                case A_FLOAT:
                    value = ap[0].a_w.w_float;
                    break;
                    
                default:
                    post("Invalid argument in parameter setting"); 
                    return;         
            }
            
            stringstream num_val;
            num_val << num + i;
            char param_name[256];
            
            switch (ndigit - count_digit(num_val.str())) {
                case 0: 
                    sprintf(param_name, "%s%s", prefix.c_str(), num_val.str().c_str());
                    break;
                case 1: 
                    sprintf(param_name, "%s %s", prefix.c_str(), num_val.str().c_str());
                    break;
                case 2: 
                    sprintf(param_name, "%s  %s", prefix.c_str(), num_val.str().c_str());
                    break;
            }
            
            res = fDSPUI.setValue(param_name, value); // Doesn't have any effect if name is unknown
        }
    // Standard parameter
    } else {
        float value = (av[0].a_type == A_LONG) ? (float)av[0].a_w.w_long : av[0].a_w.w_float;
        res = fDSPUI.setValue(name, value); // Doesn't have any effect if name is unknown
    }
    
    if (!res) {
        post("Unknown parameter : %s", (s)->s_name);
    }
}	

void faustgen::compileoptions(long inlet, t_symbol* s, long argc, t_atom* argv) 
{ 
    fDSPfactory->compileoptions(inlet, s, argc, argv);
}

void faustgen::read(long inlet, t_symbol* s)
{
    fDSPfactory->read(inlet, s);
}

void faustgen::write(long inlet, t_symbol* s)
{
    fDSPfactory->write(inlet, s);
}

// Called when saving the Max patcher, this function saves the necessary data inside the json file (faust sourcecode)
void faustgen::appendtodictionary(t_dictionary* d)
{
    fDSPfactory->appendtodictionary(d);
}

void faustgen::getfromdictionary(t_dictionary* d)
{
    fDSPfactory->getfromdictionary(d);
}

// Called when the user double-clicks on the faustgen object inside the Max patcher
void faustgen::dblclick(long inlet)
{
    // Create a popup menu inside the Max patcher
    t_jpopupmenu* popup = jpopupmenu_create(); 
    jpopupmenu_additem(popup, 0, "Edit DSP code", NULL, 0, 0, NULL);
    jpopupmenu_additem(popup, 1, "View DSP parameters", NULL, 0, 0, NULL);
    jpopupmenu_additem(popup, 2, "View compile options", NULL, 0, 0, NULL);
    jpopupmenu_additem(popup, 3, "View SVG diagram", NULL, 0, 0, NULL);
    jpopupmenu_additem(popup, 4, "View PDF documentation", NULL, 0, 0, NULL);
    jpopupmenu_additem(popup, 5, "View libraries", NULL, 0, 0, NULL);
    
    // Get mouse position
    int x,y;
    jmouse_getposition_global(&x, &y);
    t_pt coordinate;
    coordinate.x = x;
    coordinate.y = y;
    
    int choice = jpopupmenu_popup(popup, coordinate, 0);        
    
    switch (choice) {
       
        case 0: 
            // Open the text editor to allow the user to input Faust sourcecode
            display_dsp_source();
            break;
    
        case 1: 
            // Display inside the max window the current values of the module's parameters, as well as their bounds
            display_dsp_params();
            break;
            
        case 2: 
            // Display compiler options
            fDSPfactory->print_compile_options();
            break;
  
        case 3: 
            // Open the SVG diagram file inside a web browser
            display_svg();
            break;
            
        case 4: 
            // Open the PDF documentation
            display_pdf();
            break;
            
        case 5: 
            // Open the libraries
            display_libraries();
            break;

            
        default:
            break;
    }
    
    // Destroy the popup menu once this is done
    jpopupmenu_destroy(popup);
}

// Called when closing the text editor, calls for the creation of a new Faust module with the updated sourcecode
void faustgen::edclose(long inlet, char** source_code, long size)
{
    // Edclose "may" be called with an already deallocated object (like closing the patcher with a still opened editor)
    if (fDSP && fEditor) {
        fDSPfactory->update_sourcecode(size, *source_code, this);
        fEditor = 0;
    } 
}

void faustgen::update_sourcecode()
{
    // Create a new DSP instance
    create_dsp(false);
    
    // Faustgen~ state is modified...
    set_dirty();
}

// Process the signal data with the Faust module
inline void faustgen::perform(int vs, t_sample** inputs, long numins, t_sample** outputs, long numouts) 
{
    if (!fMute && fDSPfactory->try_lock()) {
        // Has to be tested again when the lock has been taken...
        if (fDSP) {
            fDSP->compute(vs, (FAUSTFLOAT**)inputs, (FAUSTFLOAT**)outputs);
        }
    
        fDSPfactory->unlock();
    } else {
        // Write null buffers to outs
        for (int i = 0; i < numouts; i++) {
            memset(outputs[i], 0, sizeof(t_sample) * vs);
        }
    }
}

// Display source code
void faustgen::display_dsp_source()
{
    if (fEditor) {
        // Editor already open, set it to to foreground
        object_attr_setchar(fEditor, gensym("visible"), 1);
    } else {
        // Create a text editor object
        fEditor = (t_object*)object_new(CLASS_NOBOX, gensym("jed"), this, 0); 
    
        // Set the text inside the text editor to be fSourceCode 
        object_method(fEditor, gensym("settext"), fDSPfactory->get_sourcecode(), gensym("utf-8"));
        object_attr_setchar(fEditor, gensym("scratch"), 1); 
        char name[256];
        snprintf(name, 256, "DSP code : %s", fDSPfactory->get_name().c_str());
        object_attr_setsym(fEditor, gensym("title"), gensym(name));
    }
}

// Display the Faust module's parameters along with their standard values
void faustgen::display_dsp_params()
{
    if (fDSPUI.itemsCount() > 0) {
        post("------------------");
    }
    
    char param[256];
    for (mspUI::iterator it = fDSPUI.begin(); it != fDSPUI.end(); ++it) {
        it->second->toString(param);
        post(param);
    };
    
    if (fDSPUI.itemsCount() > 0) {
        post("------------------");
    }
}

void faustgen::display_svg()
{
    fDSPfactory->display_svg();
}

void faustgen::display_pdf()
{
    fDSPfactory->display_pdf();
}

void faustgen::display_libraries()
{
    fDSPfactory->display_libraries();
}

void faustgen::hilight_on(const std::string& error)
{
    t_jrgba color;
    jrgba_set(&color, 1.0, 0.0, 0.0, 1.0);
    t_object* box;
    object_obex_lookup((t_object*)&m_ob, gensym("#B"), &box);
    jbox_set_color(box, &color);
    object_error_obtrusive((t_object*)&m_ob, (char*)error.c_str());
}
  
void faustgen::hilight_off()
{
    t_object* box;
    object_obex_lookup((t_object*)&m_ob, gensym("#B"), &box);
    jbox_set_color(box, &fDefaultColor);
}

void faustgen::create_dsp(bool init)
{
    if (fDSPfactory->lock()) {
        fDSP = fDSPfactory->create_dsp_aux(this);
        assert(fDSP);
        
        // Initialize User Interface (here connnection with controls)
        fDSP->buildUserInterface(&fDSPUI);
        
        // Initialize at the system's sampling rate
        fDSP->init(sys_getsr());
            
        // Setup MAX audio IO
        bool dspstate = false;
        
        if ((m_siginlets != fDSP->getNumInputs()) || (m_sigoutlets != fDSP->getNumOutputs())) {
            // Number of ins/outs have changed... possibly stop IO 
            dspstate = sys_getdspobjdspstate((t_object*)&m_ob);
            if (dspstate) {
                dsp_status("stop");
            }
        }
   
        setupIO(&faustgen::perform, fDSP->getNumInputs(), fDSP->getNumOutputs(), init); 
        
        // Possibly restart IO
        if (dspstate) {
            dsp_status("start");
        }
        
        fDSPfactory->unlock();
    } else {
        post("Mutex lock cannot be taken...");
    }
}

void faustgen::set_dirty()
{
    t_object* mypatcher;
    object_obex_lookup(&m_ob, gensym("#P"), &mypatcher); 
    jpatcher_set_dirty(mypatcher, 1);
}

t_pxobject* faustgen::check_dac()
{
    t_object *patcher, *box, *obj;
    object_obex_lookup(this, gensym("#P"), &patcher);
    
    for (box = jpatcher_get_firstobject(patcher); box; box = jbox_get_nextobject(box)) {
        obj = jbox_get_object(box);
        if (obj) {
            if ((object_classname(obj) == gensym("dac~"))
                || (object_classname(obj) == gensym("ezdac~"))
                || (object_classname(obj) == gensym("ezadc~"))
                || (object_classname(obj) == gensym("adc~"))) {
                    return (t_pxobject*)box;
            }
        }
    }
    
    return 0;
}

void faustgen::dsp_status(const char* mess)
 {
    t_pxobject* dac = 0;
    
    if ((dac = check_dac())) {
        t_atom msg[1];
        atom_setsym(msg, gensym(mess));
        object_method_typed(dac, gensym("message"), 1, msg, 0);
    } else { // Global
        object_method(gensym("dsp")->s_thing, gensym(mess));
    }
}

void faustgen::mute(long inlet, long mute)
{
    fMute = mute;
}

#ifdef WIN32
extern "C" int main(void)
#else
int main(void)
#endif
{
    // Creates an instance of Faustgen
    faustgen::makeMaxClass("faustgen~");
    post("faustgen~ v%s", FAUSTGEN_VERSION);
    post("LLVM powered Faust embedded compiler");
    post("Copyright (c) 2012-2014 Grame");
  
    // Process all messages coming to the object using a custom method
    REGISTER_METHOD_GIMME(faustgen, anything);
    
    // Process the "compileoptions" message, to add optional compilation possibilities
    REGISTER_METHOD_GIMME(faustgen, compileoptions);
    
    // Register inside Max the necessary methods
    REGISTER_METHOD_DEFSYM(faustgen, read);
    REGISTER_METHOD_DEFSYM(faustgen, write);
    REGISTER_METHOD_LONG(faustgen, mute);
    REGISTER_METHOD_CANT(faustgen, dblclick);
    REGISTER_METHOD_EDCLOSE(faustgen, edclose);
    REGISTER_METHOD_JSAVE(faustgen, appendtodictionary);
}
