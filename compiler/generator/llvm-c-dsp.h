/************************************************************************
 ************************************************************************
 FAUST Architecture File
 Copyright (C) 2003-2014 GRAME, Centre National de Creation Musicale
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
 
 ************************************************************************
 ************************************************************************/

#ifndef LLVM_DSP_C_H
#define LLVM_DSP_C_H

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

#include <stdbool.h>
#include "faust/gui/CUI.h"

#ifdef __cplusplus
extern "C"
{
#endif
    
    /* Opaque types */
	
    /*!
     \addtogroup llvmc C interface for compiling Faust code.
     @{
     */
    
    typedef struct {} llvm_dsp_factory;
    
    typedef struct {} llvm_dsp;
    
    /**
     * Get the Faust DSP factory associated with a given SHA key (created from the 'expanded' DSP source), 
     * if already allocated in the factories cache.
     *
     * @param sha_key - the SHA key for an already created factory, kept in the factory cache
     *
     * @return a valid DSP factory if one is associated with the SHA key, otherwise a null pointer.
     */
    llvm_dsp_factory* getCDSPFactoryFromSHAKey(char* sha_key);
    
    /**
     * Create a Faust DSP factory from a DSP source code as a file. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is same source code and 
     * same set of 'normalized' compilations options) will return the same (reference counted) factory pointer.
     *
     * @param filename - the DSP filename
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param target - the LLVM machine target (using empty string will take current machine settings)
     * @param error_msg - the error string to be filled, has to be 256 characters long
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3)
     *
     * @return a valid DSP factory on success, otherwise a null pointer.
     */ 
    llvm_dsp_factory* createCDSPFactoryFromFile(const char* filename, int argc, const char* argv[], 
                                                const char* target, 
                                                char* error_msg, int opt_level);
    
    /**
     * Create a Faust DSP factory from a DSP source code as a string. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is same source code and 
     * same set of 'normalized' compilations options) will return the same (reference counted) factory pointer.
     * 
     * @param name_app - the name of the Faust program
     * @param dsp_content - the Faust program as a string
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param target - the LLVM machine target (using empty string will take current machine settings)
     * @param error_msg - the error string to be filled, has to be 256 characters long
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3)
     *
     * @return a valid DSP factory on success, otherwise a null pointer.
     */ 
    llvm_dsp_factory* createCDSPFactoryFromString(const char* name_app, const char* dsp_content, int argc, const char* argv[], 
                                                  const char* target, 
                                                  char* error_msg, int opt_level);
    
    /**
     * Destroy a Faust DSP factory, that is decrements it's reference counter, possible really deleting the pointer.  
     * 
     * @param factory - the DSP factory to be deleted.
     */                                 
    void deleteCDSPFactory(llvm_dsp_factory* factory);
    
    /**
     * Destroy all Faust DSP factory kept in the libray cache. Beware : all kept factory pointer (in local variables of so...) thus become invalid.
     * 
     */                                 
    void deleteAllCDSPFactories();
    
    /**
     * Return Faust DSP factories of the library cache as an array of their SHA keys.
     * 
     * @return the Faust DSP factories (the array and it's contain has to be deallocated by the caller)
     */    
    const char** getAllCDSPFactories();
    
    /**
     * Create a Faust DSP factory from a LLVM bitcode string. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is the same LLVM bitcode string) will return 
     * the same (reference counted) factory pointer.
     * 
     * @param bit_code - the LLVM bitcode string
     * @param target - the LLVM machine target (using empty string will take current machine settings)
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3). A higher value than the one used when calling createDSPFactory can possibly be used
     *
     * @return the Faust DSP factory on success, otherwise a null pointer.
     */
    llvm_dsp_factory* readCDSPFactoryFromBitcode(const char* bit_code, const char* target, int opt_level);
    
    /**
     * Write a Faust DSP factory into a LLVM bitcode string.
     * 
     * @param factory - Faust DSP factory
     *
     * @return the LLVM bitcode as a string (to be deleted by the caller).
     */
    const char* writeCDSPFactoryToBitcode(llvm_dsp_factory* factory);
    
    /**
     * Create a Faust DSP factory from a LLVM bitcode file. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is the same LLVM bitcode file) will return 
     * the same (reference counted) factory pointer.
     * 
     * @param bit_code_path - the LLVM bitcode file pathname
     * @param target - the LLVM machine target (using empty string will takes current machine settings)
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3). A higher value than the one used when calling createDSPFactory can possibly be used
     */
    llvm_dsp_factory* readCDSPFactoryFromBitcodeFile(const char* bit_code_path, const char* target, int opt_level);
    
    /**
     * Write a Faust DSP factory into a LLVM bitcode file.
     * 
     * @param factory - the Faust DSP factory
     * @param bit_code_path - the LLVM bitcode file pathname
     *
     */
    void writeCDSPFactoryToBitcodeFile(llvm_dsp_factory* factory, const char* bit_code_path);
    
    /**
     * Create a Faust DSP factory from a LLVM IR (textual) string. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is the same LLVM IR string) will return 
     * the same (reference counted) factory pointer.
     * 
     * @param ir_code - the LLVM IR (textual) string
     * @param target - the LLVM machine target (using empty string will takes current machine settings)
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3). A higher value than the one used when calling createDSPFactory can possibly be used
     *
     * @return the Faust DSP factory on success, otherwise a null pointer.
     */
    llvm_dsp_factory* readCDSPFactoryFromIR(const char* ir_code, const char* target, int opt_level);
    
    /**
     * Write a Faust DSP factory into a LLVM IR (textual) string.
     * 
     * @param factory - the Faust DSP factory
     *
     * @return the LLVM IR (textual) as a string (to be deleted by the caller).
     */
    const char* writeCDSPFactoryToIR(llvm_dsp_factory* factory);
    
    /**
     * Create a Faust DSP factory from a LLVM IR (textual) file. Note that the library keeps an internal cache of all 
     * allocated factories so that the compilation of same DSP code (that is the same LLVM IR file) will return 
     * the same (reference counted) factory pointer.
     * 
     * @param ir_code_path - the LLVM IR (textual) file pathname
     * @param target - the LLVM machine target (using empty string will takes current machine settings)
     * @param opt_level - LLVM IR to IR optimization level (from 0 to 3). A higher value than the one used when calling createDSPFactory can possibly be used
     *
     * @return the Faust DSP factory on success, otherwise a null pointer.
     */
    llvm_dsp_factory* readCDSPFactoryFromIRFile(const char* ir_code_path, const char* target, int opt_level);
    
    /**
     * Write a Faust DSP factory into a LLVM IR (textual) file.
     * 
     * @param factory - the Faust DSP factory
     * @param ir_code_path - the LLVM bitcode file pathname.
     *
     */
    void writeCDSPFactoryToIRFile(llvm_dsp_factory* factory, const char* ir_code_path);
    
    /**
     * Call global declarations with the given meta object.
     * 
     * @param factory - the Faust DSP factory
     * @param meta - the meta object to be used.
     *
     */
    void metadataCDSPFactory(llvm_dsp_factory* factory, MetaGlue* meta);
    
    /**
     * From a DSP source file, creates a 'self-contained' DSP source string where all needed librairies have been included.
     * All compilations options are 'normalized' and included as a comment in the expanded string.
     
     * @param filename - the DSP filename
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param sha_key - the SHA key to be filled
     * @param error_msg - the error string to be filled
     *
     * @return the expanded DSP as a string on success (to be deleted by the caller), otherwise a null pointer.
     */ 
    const char* expandCDSPFromFile(const char* filename, 
                                   int argc, const char* argv[], 
                                   char* sha_key,
                                   char* error_msg);
    
    /**
     * From a DSP source string, creates a 'self-contained' DSP source string where all needed librairies have been included.
     * All compilations options are 'normalized' and included as a comment in the expanded string.
     
     * @param name_app - the name of the Faust program
     * @param dsp_content - the Faust program as a string
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param sha_key - the SHA key to be filled
     * @param error_msg - the error string to be filled
     *
     * @return the expanded DSP as a string on success (to be deleted by the caller), otherwise a null pointer.
     */ 
    const char* expandCDSPFromString(const char* name_app, 
                                     const char* dsp_content, 
                                     int argc, const char* argv[], 
                                     char* sha_key,
                                     char* error_msg);
    
    /**
     * From a DSP source file, generates auxillary files : SVG, XML, ps... depending of the 'argv' parameters.
     
     * @param filename - the DSP filename
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param error_msg - the error string to be filled
     *
     * @return true if compilation succedeed, false and an error_msg in case of failure.
     */ 
    bool generateCAuxFilesFromFile(const char* filename, int argc, const char* argv[], char* error_msg);
    
    /**
     * From a DSP source file, generates auxillary files : SVG, XML, ps... depending of the 'argv' parameters.
     
     * @param name_app - the name of the Faust program
     * @param dsp_content - the Faust program as a string
     * @param argc - the number of parameters in argv array
     * @param argv - the array of parameters
     * @param error_msg - the error string to be filled
     *
     * @return true if compilation succedeed, false and an error_msg in case of failure.
     */ 
    bool generateCAuxFilesFromString(const char* name_app, const char* dsp_content, int argc, const char* argv[], char* error_msg);
    
    /**
     * Instance functions.
     */
    
    void metadataCDSPInstance(llvm_dsp* dsp, MetaGlue* meta);
    
    int getNumInputsCDSPInstance(llvm_dsp* dsp);
    
    int getNumOutputsCDSPInstance(llvm_dsp* dsp);
    
    void initCDSPInstance(llvm_dsp* dsp, int samplingFreq);
    
    void buildUserInterfaceCDSPInstance(llvm_dsp* dsp, UIGlue* interface);
    
    void computeCDSPInstance(llvm_dsp* dsp, int count, FAUSTFLOAT** input, FAUSTFLOAT** output);
    
    /**
     * Create a Faust DSP instance.
     * 
     * @param factory - the Faust DSP factory
     * 
     * @return the Faust DSP instance on success, otherwise a null pointer.
     */
    llvm_dsp* createCDSPInstance(llvm_dsp_factory* factory);
    
    /**
     * Destroy a Faust DSP instance.
     * 
     * @param dsp - the DSP instance to be deleted.
     */ 
    void deleteCDSPInstance(llvm_dsp* dsp);
    
#ifdef __cplusplus
}
#endif

/*!
 @}
 */

#endif
