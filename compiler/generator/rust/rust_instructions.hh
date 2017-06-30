/************************************************************************
 ************************************************************************
    FAUST compiler
    Copyright (C) 2017 GRAME, Centre National de Creation Musicale
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

#ifndef _RUST_INSTRUCTIONS_H
#define _RUST_INSTRUCTIONS_H

#include "text_instructions.hh"

using namespace std;

// Visitor used to initialize fields into the DSP constructor
struct RustInitFieldsVisitor : public DispatchVisitor {

    std::ostream* fOut;
    int fTab;
    
    RustInitFieldsVisitor(std::ostream* out, int tab = 0):fOut(out), fTab(tab) {}
    
    virtual void visit(DeclareVarInst* inst)
    {
        ArrayTyped* array_type = dynamic_cast<ArrayTyped*>(inst->fType);
        Typed::VarType type = inst->fType->getType();
        
        tab(fTab, *fOut);
        *fOut << inst->fAddress->getName() << ": ";
        
        if (array_type) {
            if (isIntPtrType(type)) {
                *fOut << "[0; " << array_type->fSize << "],";
            } else if (isRealPtrType(type)) {
                *fOut << "[0.0; " << array_type->fSize << "],";
            }
        } else {
            if (isIntType(type)) {
                *fOut << "0,";
            } else if (isRealType(type)) {
                *fOut << "0.0,";
            }
        }
    }
    
 };

class RustInstVisitor : public TextInstVisitor {
    
    private:
    
        /*
         Global functions names table as a static variable in the visitor
         so that each function prototype is generated as most once in the module.
         */
        static map <string, int> gFunctionSymbolTable;
        map <string, string> fMathLibTable;

    public:

        RustInstVisitor(std::ostream* out, const string& structname, int tab = 0)
            :TextInstVisitor(out, ".", new RustStringTypeManager(FLOATMACRO, "&"), tab)
        {
            /*
            fTypeManager->fTypeDirectTable[Typed::kObj] = structname;
            fTypeManager->fTypeDirectTable[Typed::kObj_ptr] = structname + "*";
            */
            
            fTypeManager->fTypeDirectTable[Typed::kObj] = "";
            fTypeManager->fTypeDirectTable[Typed::kObj_ptr] = "";
            
            // Integer version
            fMathLibTable["abs"] = "i32::abs";
            fMathLibTable["min_i"] = "std::cmp::min";
            fMathLibTable["max_i"] = "std::cmp::max";
            
            // Float version
            fMathLibTable["fabsf"] = "f32::abs";
            fMathLibTable["acosf"] = "f32::acos";
            fMathLibTable["asinf"] = "f32::asin";
            fMathLibTable["atanf"] = "f32::atan";
            fMathLibTable["atan2f"] = "f32::atan2";
            fMathLibTable["ceilf"] = "f32::ceil";
            fMathLibTable["cosf"] = "f32::cos";
            fMathLibTable["expf"] = "f32::exp";
            fMathLibTable["floorf"] = "f32::floor";
            fMathLibTable["fmodf"] = "f32::fmod";
            fMathLibTable["logf"] = "f32::log";
            fMathLibTable["log10f"] = "f32::log10";
            fMathLibTable["max_f"] = "f32::max";
            fMathLibTable["min_f"] = "f32::min";
            fMathLibTable["powf"] = "f32::powf";
            fMathLibTable["remainderf"] = "manual";        // Manually generated : TODO
            fMathLibTable["roundf"] = "f32::round";
            fMathLibTable["sinf"] = "f32::sin";
            fMathLibTable["sqrtf"] = "f32::sqrt";
            fMathLibTable["tanf"] = "f32::tan";
            
            // Double version
            fMathLibTable["fabs"] = "f64::abs";
            fMathLibTable["acos"] = "f64::acos";
            fMathLibTable["asin"] = "f64::asin";
            fMathLibTable["atan"] = "f64::atan";
            fMathLibTable["atan2"] = "f64::atan2";
            fMathLibTable["ceil"] = "f64::ceil";
            fMathLibTable["cos"] = "f64::cos";
            fMathLibTable["exp"] = "f64::exp";
            fMathLibTable["floor"] = "f64::floor";
            fMathLibTable["fmod"] = "f64::fmod";
            fMathLibTable["log"] = "f64::log";
            fMathLibTable["log10"] = "f32::log10";
            fMathLibTable["max_"] = "f64::max";
            fMathLibTable["min_"] = "f64::min";
            fMathLibTable["pow"] = "f64::powf";
            fMathLibTable["remainder"] = "manual";         // Manually generated : TODO
            fMathLibTable["round"] = "f64::round";
            fMathLibTable["sin"] = "f64::sin";
            fMathLibTable["sqrt"] = "f64::sqrt";
            fMathLibTable["tan"] = "f64::tan";
        }

        virtual ~RustInstVisitor()
        {}

        virtual void visit(AddMetaDeclareInst* inst)
        {
            // Special case
            if (inst->fZone == "0") {
                *fOut << "ui_interface->declare(ui_interface->uiInterface, " << inst->fZone << ", " << quote(inst->fKey) << ", " << quote(inst->fValue) << ")";
            } else {
                *fOut << "ui_interface->declare(ui_interface->uiInterface, " << "&dsp->" << inst->fZone <<", " << quote(inst->fKey) << ", " << quote(inst->fValue) << ")";
            }
            EndLine();
        }

        virtual void visit(OpenboxInst* inst)
        {
            string name;
            switch (inst->fOrient) {
                case 0:
                    name = "ui_interface->openVerticalBox("; break;
                case 1:
                    name = "ui_interface->openHorizontalBox("; break;
                case 2:
                    name = "ui_interface->openTabBox("; break;
            }
            *fOut << name << "ui_interface->uiInterface, " << quote(inst->fName) << ")";
            EndLine();
        }

        virtual void visit(CloseboxInst* inst)
        {
            *fOut << "ui_interface->closeBox(ui_interface->uiInterface);"; tab(fTab, *fOut);
        }
        virtual void visit(AddButtonInst* inst)
        {
            string name;
            if (inst->fType == AddButtonInst::kDefaultButton) {
                name = "ui_interface->addButton("; 
            } else {
                name = "ui_interface->addCheckButton("; 
            }
            *fOut << name << "ui_interface->uiInterface, " << quote(inst->fLabel) << ", " << "&dsp->" << inst->fZone << ")";
            EndLine();
        }

        virtual void visit(AddSliderInst* inst)
        {
            string name;
            switch (inst->fType) {
                case AddSliderInst::kHorizontal:
                    name = "ui_interface->addHorizontalSlider("; break;
                case AddSliderInst::kVertical:
                    name = "ui_interface->addVerticalSlider("; break;
                case AddSliderInst::kNumEntry:
                    name = "ui_interface->addNumEntry("; break;
            }
            *fOut << name << "ui_interface->uiInterface, " << quote(inst->fLabel) << ", " 
            << "&dsp->" << inst->fZone << ", " << checkReal(inst->fInit) << ", " 
            << checkReal(inst->fMin) << ", " << checkReal(inst->fMax) << ", " << checkReal(inst->fStep) << ")";
            EndLine();
        }

        virtual void visit(AddBargraphInst* inst)
        {
            string name;
            switch (inst->fType) {
                case AddBargraphInst::kHorizontal:
                    name = "ui_interface->addHorizontalBargraph("; break;
                case AddBargraphInst::kVertical:
                    name = "ui_interface->addVerticalBargraph("; break;
            }
            *fOut << name << "ui_interface->uiInterface, " << quote(inst->fLabel) 
            << ", " << "&dsp->" << inst->fZone << ", "<< checkReal(inst->fMin) 
            << ", " << checkReal(inst->fMax) << ")";          
            EndLine();
        }

        virtual void visit(DeclareVarInst* inst)
        {
            if (inst->fAddress->getAccess() & Address::kStaticStruct) {
                *fOut << "static ";
            }

            if (inst->fAddress->getAccess() & Address::kVolatile) {
                *fOut << "volatile ";
            }
            
            if (inst->fAddress->getAccess() & Address::kStruct) {
                // Nothing
            } else {
                *fOut << "let mut ";
            }
            
            *fOut << fTypeManager->generateType(inst->fType, inst->fAddress->getName());
            if (inst->fValue) {
                *fOut << " = "; inst->fValue->accept(this);
            }
            
            if (inst->fAddress->getAccess() & Address::kStruct) {
                // To improve
                if (fFinishLine) {
                    *fOut << ",";
                    tab(fTab, *fOut);
                }
            } else {
                EndLine();
            }
        }

        virtual void visit(DeclareFunInst* inst)
        {
            // Already generated
            if (gFunctionSymbolTable.find(inst->fName) != gFunctionSymbolTable.end()) {
                return;
            } else {
                gFunctionSymbolTable[inst->fName] = 1;
            }
            
            /*
            // Defined as macro in the architecture file...
            if (startWith(inst->fName, "min") || startWith(inst->fName, "max")) {
                return;
            }
            */
      
            // Prototype
            if (inst->fType->fAttribute & FunTyped::kInline) {
                *fOut << "inline ";
            }
            
            if (inst->fType->fAttribute & FunTyped::kLocal) {
                *fOut << "static ";
            }
            
            // Only generates additional functions
            if (fMathLibTable.find(inst->fName) == fMathLibTable.end()) {
                // Prototype
                *fOut << "pub fn " << inst->fName;
                generateFunDefArgs(inst);
                generateFunDefBody(inst);
            }
        }
    
        /*
        virtual void generateFunDefArgs(DeclareFunInst* inst)
        {
            *fOut << "(";
            list<NamedTyped*>::const_iterator it;
            int size = inst->fType->fArgsTypes.size(), i = 0;
            for (it = inst->fType->fArgsTypes.begin(); it != inst->fType->fArgsTypes.end(); it++, i++) {
                *fOut << fTypeManager->generateType((*it));
                if (i < size - 1) *fOut << ", ";
            }
        }
        */
    
        virtual void generateFunDefBody(DeclareFunInst* inst)
        {
            *fOut << ") -> " << fTypeManager->generateType(inst->fType->fResult);
            if (inst->fCode->fCode.size() == 0) {
                *fOut << ";" << endl;  // Pure prototype
            } else {
                // Function body
                *fOut << " {";
                fTab++;
                tab(fTab, *fOut);
                inst->fCode->accept(this);
                fTab--;
                tab(fTab, *fOut);
                *fOut << "}";
                tab(fTab, *fOut);
            }
        }
    
        virtual void visit(RetInst* inst)
        {
            if (inst->fResult) {
                //*fOut << "return ";
                inst->fResult->accept(this);
            } else {
                *fOut << "return";
                EndLine();
            }
        }
    
        virtual void visit(NamedAddress* named)
        {
            if (named->getAccess() & Address::kStruct) {
                if (named->getAccess() & Address::kReference) {
                    *fOut << "&mut self.";
                } else {
                    *fOut << "self.";
                }
            }
            *fOut << named->fName;
        }
    
        virtual void visit(IndexedAddress* indexed)
        {
            indexed->fAddress->accept(this);
            if (dynamic_cast<IntNumInst*>(indexed->fIndex)) {
                *fOut << "["; indexed->fIndex->accept(this); *fOut << "]";
            } else {
                *fOut << "["; indexed->fIndex->accept(this); *fOut << " as usize]"; // Array index expression casted to 'usize' type
            }
        }
    
        virtual void visit(LoadVarAddressInst* inst)
        {
            *fOut << "&"; inst->fAddress->accept(this);
        }
    
        virtual void visit(CastNumInst* inst)
        {
            *fOut << "(";
            inst->fInst->accept(this);
            *fOut << " as " << fTypeManager->generateType(inst->fType);
            *fOut << ")";
        }

        // Generate standard funcall (not 'method' like funcall...)
        virtual void visit(FunCallInst* inst)
        {
            // Integer and real min/max are mapped on polymorphic ones
            /*
            string name;
            if (startWith(inst->fName, "min")) {
                name = "min";
            } else if (startWith(inst->fName, "max")) {
                name = "max";
            } else {
                name = inst->fName;
            }
            */
            
            /*
            if (fMathLibTable.find(inst->fName) != fMathLibTable.end()) {
                *fOut << fMathLibTable[inst->fName] << "(";
            } else {
                *fOut << inst->fName << "(";
            }
            // Compile parameters
            generateFunCallArgs(inst->fArgs.begin(), inst->fArgs.end(), inst->fArgs.size());
            *fOut << ")";
             
            */
            
            string fun_name;
            if (fMathLibTable.find(inst->fName) != fMathLibTable.end()) {
                fun_name = fMathLibTable[inst->fName];
            } else {
                fun_name = inst->fName;
            }
            generateFunCall(inst, fun_name);
        }
    
        virtual void visit(Select2Inst* inst)
        {
            *fOut << "if (";
            inst->fCond->accept(this);
            *fOut << " as i32 == 1) { ";
            inst->fThen->accept(this);
            *fOut << " } else { ";
            inst->fElse->accept(this);
            *fOut << " }";
        }
    
        virtual void visit(IfInst* inst)
        {
            *fOut << "if (";
            inst->fCond->accept(this);
            *fOut << " as i32 == 1) { ";
            fTab++;
            tab(fTab, *fOut);
            inst->fThen->accept(this);
            fTab--;
            tab(fTab, *fOut);
            if (inst->fElse->fCode.size() > 0) {
                *fOut << "} else {";
                fTab++;
                tab(fTab, *fOut);
                inst->fElse->accept(this);
                fTab--;
                tab(fTab, *fOut);
                *fOut << "}";
            } else {
                *fOut << "}";
            }
            tab(fTab, *fOut);
        }
   
        virtual void visit(ForLoopInst* inst)
        {
            // Don't generate empty loops...
            if (inst->fCode->size() == 0) return;
            
            inst->fInit->accept(this);
            *fOut << "loop {";
                fTab++;
                tab(fTab, *fOut);
                inst->fCode->accept(this);
                inst->fIncrement->accept(this);
                *fOut << "if "; inst->fEnd->accept(this); *fOut << " { continue; } else { break; }";
                 fTab--;
            tab(fTab, *fOut);
            *fOut << "}";
            tab(fTab, *fOut);
        }
    
        virtual void visit(::SwitchInst* inst)
        {
            *fOut << "match ("; inst->fCond->accept(this); *fOut << ") {";
                fTab++;
                tab(fTab, *fOut);
                list<pair <int, BlockInst*> >::const_iterator it;
                for (it = inst->fCode.begin(); it != inst->fCode.end(); it++) {
                    if ((*it).first == -1) { // -1 used to code "default" case
                        *fOut << "_ => {";
                    } else {
                        *fOut << (*it).first << " => {";
                    }
                        fTab++;
                        tab(fTab, *fOut);
                        ((*it).second)->accept(this);
                        /*
                        if (!((*it).second)->hasReturn()) {
                            *fOut << "break;";
                        }
                        */
                        fTab--;
                        tab(fTab, *fOut);
                    *fOut << "},";
                    tab(fTab, *fOut);
                }
                fTab--;
                tab(fTab, *fOut);
                *fOut << "} ";
            tab(fTab, *fOut);
        }
    
        static void cleanup() { gFunctionSymbolTable.clear(); }
};

#endif
