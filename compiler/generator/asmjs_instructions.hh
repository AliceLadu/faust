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

#ifndef _ASMJAVASCRIPT_INSTRUCTIONS_H
#define _ASMJAVASCRIPT_INSTRUCTIONS_H

using namespace std;

#include "text_instructions.hh"

class ASMJAVAScriptInstVisitor : public TextInstVisitor {

    private:

        static map <string, int> gFunctionSymbolTable;      // Global functions names
        map <string, string> fMathLibTable;
        Typed::VarType fCurType;

    public:

        ASMJAVAScriptInstVisitor(std::ostream* out, int tab = 0):TextInstVisitor(out, ".", tab), fCurType(Typed::kNoType)
        {
            fMathLibTable["abs"] = "Math.abs";
            fMathLibTable["absf"] = "Math.abs";
            fMathLibTable["fabsf"] = "Math.abs";
            fMathLibTable["acosf"] = "Math.acos";
            fMathLibTable["asinf"] = "Math.asin";
            fMathLibTable["atanf"] = "Math.atan";
            fMathLibTable["atan2f"] = "Math.atan2";
            fMathLibTable["ceilf"] = "Math.ceil";
            fMathLibTable["cosf"] = "Math.cos";
            fMathLibTable["expf"] = "Math.exp";
            fMathLibTable["floorf"] = "Math.floor";
            fMathLibTable["fmodf"] = "function fmod(a, b) { return a % b; }";
            fMathLibTable["logf"] = "Math.log";
            fMathLibTable["log10f"] = "function log10(a) { return Math.log(a)/Math.log(10); }";
            fMathLibTable["max"] = "Math.max";
            fMathLibTable["min"] = "Math.min";
            fMathLibTable["powf"] = "Math.pow";
            fMathLibTable["roundf"] = "Math.round";
            fMathLibTable["sinf"] = "Math.sin";
            fMathLibTable["sqrtf"] = "Math.sqrt";
            fMathLibTable["tanf"] = "Math.tan";
        }

        virtual ~ASMJAVAScriptInstVisitor()
        {}

        virtual void visit(AddMetaDeclareInst* inst)
        {
            *fOut << "ui_interface.declare(\"" << inst->fZone << "\", \"" << inst->fKey << "\", \"" <<  inst->fValue << "\")";
            EndLine();
        }

        virtual void visit(OpenboxInst* inst)
        {
            string name;
            switch (inst->fOrient) {
                case 0:
                    name = "ui_interface.openVerticalBox"; break;
                case 1:
                    name = "ui_interface.openHorizontalBox"; break;
                case 2:
                    name = "ui_interface.openTabBox"; break;
            }
            *fOut << name << "(" << "\"" << inst->fName << "\"" << ")";
            EndLine();
        }

        virtual void visit(CloseboxInst* inst)
        {
            *fOut << "ui_interface.closeBox();"; tab(fTab, *fOut);
        }
        
        virtual void visit(AddButtonInst* inst)
        {
            if (inst->fType == AddButtonInst::kDefaultButton) {
                *fOut << "ui_interface.addButton(" << "\"" << inst->fLabel << "\"" << ", ";
            } else {
                *fOut << "ui_interface.addCheckButton(" << "\"" << inst->fLabel << "\"" << ", ";
            }
            
            *fOut << "function handler(obj) { function setval(val) { obj." << inst->fZone << " = val; } return setval; }(this))";
            EndLine();
        }
        
        virtual void visit(AddSliderInst* inst)
        {
            string name;
            switch (inst->fType) {
                case AddSliderInst::kHorizontal:
                    name = "ui_interface.addHorizontalSlider"; break;
                case AddSliderInst::kVertical:
                    name = "ui_interface.addVerticalSlider"; break;
                case AddSliderInst::kNumEntry:
                    name = "ui_interface.addNumEntry"; break;
            }
            *fOut << name << "(" << "\"" << inst->fLabel << "\"" << ", ";
            *fOut << "function handler(obj) { function setval(val) { obj." << inst->fZone << " = val; } return setval; }(this)";
            *fOut << ", " << inst->fInit << ", " << inst->fMin << ", " << inst->fMax << ", " << inst->fStep << ")";
            EndLine();
        }

        virtual void visit(AddBargraphInst* inst)
        {
            string name;
            switch (inst->fType) {
                case AddBargraphInst::kHorizontal:
                    name = "ui_interface.addHorizontalBargraph"; break;
                case AddBargraphInst::kVertical:
                    name = "ui_interface.addVerticalBargraph"; break;
            }
            *fOut << name << "(" << "\"" << inst->fLabel << "\"" << ", ";
            *fOut << "function handler(obj) { function setval(val) { obj." << inst->fZone << " = val; } return setval; }(this)";
            *fOut << ", " << inst->fMin << ", " << inst->fMax << ")";
            EndLine();
        }

        virtual void visit(LabelInst* inst)
        {
            // Empty
        }

        virtual void visit(DeclareVarInst* inst)
        {
            string prefix = (inst->fAddress->getAccess() & Address::kStruct) ? "this." : "var ";

            if (inst->fValue) {
                *fOut << prefix << inst->fAddress->getName() << " = "; inst->fValue->accept(this);
            } else {
                ArrayTyped* array_typed = dynamic_cast<ArrayTyped*>(inst->fType);
                if (array_typed && array_typed->fSize > 1) {
                    string type = (array_typed->fType->getType() == Typed::kFloat) ? "Float32Array" : "Int32Array";
                    *fOut << prefix << inst->fAddress->getName() << " = new " << type << "(" << array_typed->fSize << ")";
                } else {
                    *fOut << prefix << inst->fAddress->getName();
                }
            }
            EndLine();
        }
        
        virtual void generateFunArgs(DeclareFunInst* inst)
        {
            *fOut << "(";
            list<NamedTyped*>::const_iterator it;
            int size = inst->fType->fArgsTypes.size(), i = 0;
            for (it = inst->fType->fArgsTypes.begin(); it != inst->fType->fArgsTypes.end(); it++, i++) {
                *fOut << (*it)->fName;
                if (i < size - 1) *fOut << ", ";
            }
        }
  
        virtual void generateFunDefArgs(DeclareFunInst* inst)
        {
            *fOut << "(";
            list<NamedTyped*>::const_iterator it;
            int size = inst->fType->fArgsTypes.size(), i = 0;
            for (it = inst->fType->fArgsTypes.begin(); it != inst->fType->fArgsTypes.end(); it++, i++) {
                *fOut << (*it)->fName;
                if (i < size - 1) *fOut << ", ";
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
            
            // Do not declare Math library functions
            if (fMathLibTable.find(inst->fName) != fMathLibTable.end()) {
                return;
            }
        
            // Prototype
            *fOut << "this." << generateFunName(inst->fName) << " = " << "function";
            generateFunDefArgs(inst);
            generateFunDefBody(inst);
        }
        
        virtual void visit(LoadVarInst* inst)
        {
            TextInstVisitor::visit(inst);
             
            if (gGlobal->gVarTypeTable.find(inst->getName()) != gGlobal->gVarTypeTable.end()) {
                fCurType = gGlobal->gVarTypeTable[inst->getName()]->getType();
                if (dynamic_cast<IndexedAddress*>(inst->fAddress)) {
                    fCurType = Typed::getTypeFromPtr(fCurType);
                }
            } else {
                fCurType = Typed::kNoType;
            }
        }
        
        virtual void visit(NamedAddress* named)
        {   
            if (named->getAccess() & Address::kStruct) {
                *fOut << "this.";
            }
            *fOut << named->fName;
        }
  
        virtual void visit(LoadVarAddressInst* inst)
        {
           // Not implemented in JavaScript
            assert(false);
        }
                
        // No .f syntax for float in JS
        virtual void visit(FloatNumInst* inst)
        {
            *fOut << inst->fNum;
            fCurType = Typed::kFloat;
        }
        
        virtual void visit(IntNumInst* inst)
        {
            *fOut << inst->fNum;
            fCurType = Typed::kInt;
        }
        
        virtual void visit(BoolNumInst* inst)
        {
            *fOut << inst->fNum;
            fCurType = Typed::kBool;
        }
        
        // No . syntax for double in JS
        virtual void visit(DoubleNumInst* inst)
        {
            *fOut << inst->fNum;
            fCurType = Typed::kInt;
        }

        virtual void visit(CastNumInst* inst)
        {
            // No explicit cast generation
            inst->fInst->accept(this);
        }
       
        virtual void visit(FunCallInst* inst)
        {
            string fun_name = (fMathLibTable.find(inst->fName) != fMathLibTable.end()) ? fMathLibTable[inst->fName] : inst->fName;
            generateFunCall(inst, fun_name);
        }

};

#endif
