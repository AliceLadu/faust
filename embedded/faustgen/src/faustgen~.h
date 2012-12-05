/************************************************************************
    FAUST Architecture File
    Copyright (C) 2010-2012 GRAME, Centre National de Creation Musicale
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

#ifndef faustgen__h
#define faustgen__h

#define FAUSTFLOAT double
#define MSP64 1

/* link with  */
#include <iostream>
#include <fstream>
#include <sstream> 
#include <string> 
#include <set> 
#include <vector> 
#include <map> 

#include "faust/llvm-dsp.h"
#include "maxcpp5.h"

#ifndef WIN32
#include <libgen.h>
#endif

#include "mspUI.h"
#include "jpatcher_api.h"
#include "jgraphics.h"
#include "ext_drag.h"

#define DEFAULT_SOURCE_CODE "import(\"math.lib\"); \nimport(\"maxmsp.lib\"); \nimport(\"music.lib\"); \nimport(\"oscillator.lib\"); \nimport(\"reduce.lib\"); \nimport(\"filter.lib\"); \nimport(\"effect.lib\"); \n \nprocess=_,_;"
#define FAUSTGEN_VERSION "0.82b"
#define FAUST_PDF_DOCUMENTATION "faust-quick-reference.pdf"

#ifdef __APPLE__
    #include "bench-llvm.cpp"
    #define FAUST_LIBRARY_PATH "/Contents/Resources/"
    #define FAUST_DRAW_PATH "/var/tmp/"
    #define LLVM_MACHINE_TARGET "i386-apple-darwin10.6.0"
#endif

#ifdef WIN32
    #define FAUST_LIBRARY_PATH "\\faustgen-resources\\"
    #define FAUST_DRAW_PATH "\\faustgen-resources\\"
    #define LLVM_MACHINE_TARGET ""
#endif

const char* TEXT_APPL_LIST[] = {"Smultron", "TextWrangler", "TextExit", "" };

class default_llvm_dsp : public llvm_dsp {

    private:
    
        int fNumInputs;
        int fNumOutputs;
               
    public:
    
        default_llvm_dsp(int ins, int outs):fNumInputs(ins), fNumOutputs(outs)
        {}
      
        virtual int getNumInputs() { return fNumInputs; }
        virtual int getNumOutputs() { return fNumOutputs; }

        void classInit(int samplingFreq) {}
        virtual void instanceInit(int samplingFreq) {}
        virtual void init(int samplingFreq) {}

        virtual void buildUserInterface(UI* ui) {}
    
        virtual void compute(int count, FAUSTFLOAT** input, FAUSTFLOAT** output)
        {
            // Write null buffers to outs
            for (int i = 0; i < fNumOutputs; i++) {
                 memset(output[i], 0, sizeof(t_sample) * count);
            }
        }
     
};

//===================
// Faust DSP Factory
//===================

class faustgen;

class faustgen_factory {

     private:
      
        set<faustgen*> fInstances;      // set of all DSP 
        llvm_dsp_factory* fDSPfactory;  // pointer to the LLVM Faust factory
   
        long fSourceCodeSize;           // length of source code string
        char** fSourceCode;             // source code string
        
        long fBitCodeSize;              // length of the bitcode string
        char** fBitCode;                // bitcode string
        
        string fLibraryPath;            // path towards the faust libraries
        string fDrawPath;               // path where to put SVG files
                 
        int fFaustNumber;               // faustgen object's number inside the patcher
        
        faustgen* fUpdateInstance;      // the instance that inited an update
        
        string fName;                   // name of the DSP group
           
        t_systhread_mutex fDSPMutex;    // mutex to protect RT audio thread when recompiling DSP
     
        vector<string> fCompileOptions; // Faust compiler options
        
        int m_siginlets;
        int m_sigoutlets;
        
        bool open_file(const char* file);
        bool open_file(const char* appl, const char* file);
        
    public:
    
        faustgen_factory(const string& name);
        
        ~faustgen_factory();
            
        llvm_dsp_factory* create_factory_from_bitcode();
        llvm_dsp_factory* create_factory_from_sourcecode(faustgen* instance);
        llvm_dsp* create_dsp_default(int ins, int outs);
        llvm_dsp* create_dsp_aux(faustgen* instance);
     
        void free_dsp_factory();
        void free_sourcecode();
        void free_bitcode();
        
        void default_compile_options();
        void print_compile_options();
    
        void getfromdictionary(t_dictionary* d);
        void appendtodictionary(t_dictionary* d);
        
        int get_number() { return fFaustNumber; }
        string get_name() { return fName; }
        
        void read(long inlet, t_symbol* s);
        
        char* get_sourcecode() { return *fSourceCode; }
        
        void update_sourcecode(int size, char* source_code, faustgen* instance);
        
        void compileoptions(long inlet, t_symbol* s, long argc, t_atom* argv);
           
        // Compile DSP with -svg option and display the SVG files
        bool try_open_svg();
		void open_svg();
        void display_svg();
        void display_pdf();
        void display_libraries();
        void display_libraries_aux(const char* lib);
        
        void add_instance(faustgen* dsp) { fInstances.insert(dsp); }
        void remove_instance(faustgen* dsp)  
        { 
            fInstances.erase(dsp); 
            
            // Last instance : remove factory from global table and commit suicide...
            if (fInstances.size() == 0) {
                gFactoryMap.erase(fName);
                delete this;
            }
        }
        
        bool try_lock() { return systhread_mutex_trylock(fDSPMutex) == MAX_ERR_NONE; }
        bool lock() { return systhread_mutex_lock(fDSPMutex) == MAX_ERR_NONE; }
        void unlock() { systhread_mutex_unlock(fDSPMutex); }
    
        static int gFaustCounter;       // global variable to count the number of faustgen objects inside the patcher
      
        static map<string, faustgen_factory*> gFactoryMap;
};

//====================
// Faust DSP Instance
//====================

class faustgen : public MspCpp5<faustgen> {

    friend class faustgen_factory;
    
    private:
    
        faustgen_factory* fDSPfactory;

        mspUI fDSPUI;               // DSP UI
        
        // volatile 
        llvm_dsp* fDSP;             // pointer to the LLVM Faust dsp
        
        t_object* fEditor;          // text editor object
           
        bool fMute;                 // DSP mute state
        
        t_jrgba fDefaultColor;      // Color od the object to be used when restoring default color
         
        // Display DSP text source
        void display_dsp_source();
         
        // Display the Faust module's parameters along with their standard values
        void display_dsp_params();
        
        // Compile DSP with -svg option and display the SVG files
        bool try_open_svg();
        void display_svg();
        void display_pdf();
        void display_libraries();
         
        // Create the Faust LLVM based DSP
        void create_dsp(bool init);
        
        void free_dsp();
         
        void set_dirty();
        
        void dsp_status(const char* mess);
        t_pxobject* check_dac();
        
        bool allocate_factory(const string& effect_name);
       
    public:
        
        faustgen() 
        {
            faustgen(gensym("faustgen~"), NULL, NULL);
        }    
        
        faustgen(t_symbol* sym, long ac, t_atom* av);
        
        void update_sourcecode();
            
        void hilight_on(char* error);
        void hilight_off();
           
        // Called upon deleting the object inside the patcher
        ~faustgen();
         
        // Called upon sending the object a message inside the max patcher
        // Allows to set a value to the Faust module's parameter 
        void anything(long inlet, t_symbol*s, long ac, t_atom*av);
         
        void compileoptions(long inlet, t_symbol*s, long argc, t_atom* argv);
         
        void read(long inlet, t_symbol* s);
        
        void mute(long inlet, long mute);
         
        // Called when saving the Max patcher
        // This function saves the necessary data inside the json file (faust sourcecode)
        void appendtodictionary(t_dictionary* d);
        
        void getfromdictionary(t_dictionary *d);
     
        // Called when the user double-clicks on the faustgen object inside the Max patcher
        void dblclick(long inlet);
        
        // Called when closing the text editor, calls for the creation of a new Faust module with the updated sourcecode
        void edclose(long inlet, char **text, long size);
          
        // Process the signal data with the Faust module
        void perform(int vs, t_sample ** inputs, long numins, t_sample ** outputs, long numouts);
};

#endif
