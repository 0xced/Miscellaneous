/*

fixobjc2.idc ... IDA Pro 6.0 script to fix ObjC ABI 2.0 for iPhoneOS binaries.

Copyright (C) 2011  KennyTM~
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <idc.idc>
#define RO_META 1

/* offsetize: Convert the whole segment with name 'name' into a series of off-
              sets
 */
static offsetize(name) {
    auto base, ea;
    base = SegByBase(SegByName(name));
    if (base != BADADDR) {
        for (ea = SegStart(base); ea != SegEnd(base); ea = ea + 4) {
            OpOff(ea, 0, 0);
        }
    }
}

/* functionize: Make 'ea' an Objective-C method
                 ea      - The address
                 is_meta - Is this a class method?
                 cls     - Class name
                 selname - Selector name
                 sel     - Address of the selector (currently always -1)
 */
static functionize (ea, is_meta, cls, selname, sel) {
    auto is_thumb, head_ea, ea_flags;
    is_thumb = ea & 1;
    ea = ea & ~1;
    ea_flags = GetFlags(ea);
    if (!isCode(ea_flags) || GetReg(ea, "T") != is_thumb) {
        head_ea = !isHead(ea_flags) ? PrevHead(ea, 0) : ea;
        MakeUnkn(head_ea, DOUNK_EXPAND);
        SetRegEx(ea, "T", is_thumb, SR_autostart);
        MakeCode(ea);
    }
    MakeFunction(ea, BADADDR);
    MakeName(ea, (is_meta ? "@" : "") + cls + "." + selname);
    SetFunctionCmt(ea, (is_meta ? "+" : "-") + "[" + cls + " " + selname + "]", 1);
}

/* methodize: Make 'm_ea' is method_list_t.
               m_ea    - The address
               is_meta - Are these class methods?
               cl_name - Class name
 */ 
static methodize (m_ea, is_meta, cl_name) {
    auto m_size, m_count, m_i, m_sname, m_cname;
    auto m_sname_ptr, m_sel;

    if (m_ea <= 0)
        return;
        
    m_sel = -1;

    m_size = Dword(m_ea);
    m_count = Dword(m_ea + 4);
    MakeStruct(m_ea, "method_list_t");
    MakeName(m_ea, cl_name + (is_meta ? "_$classMethods" : "_$methods"));
    m_ea = m_ea + 8;
    for (m_i = 0; m_i < m_count; m_i = m_i + 1) {
        MakeStruct(m_ea, "method_t");
        m_sname_ptr = Dword(m_ea);
        /* find which selector is referencing this text. */
        if (0) {
        m_sel = DfirstB(m_sname_ptr);
        while (m_sel != -1 && SegName(m_sel) != "__objc_selrefs")
            m_sel = DnextB(m_sname_ptr, m_sel);
        }
        
        m_sname = GetString(m_sname_ptr, -1, ASCSTR_C);
        functionize(Dword(m_ea + 8), is_meta, cl_name, m_sname, m_sel);
        m_ea = m_ea + m_size;
    }
}

/* classize: Make 'c_ea' a class_t and analyze its methods. */
static classize (c_ea, is_meta) {
    auto cd_ea, cl_name, c_name, i_ea, i_count, i_size, i_i, i_sname;
    
    MakeStruct(c_ea, "class_t");
    cd_ea = Dword(c_ea + 16);
    MakeStruct(cd_ea, "class_ro_t");
    cl_name = GetString(Dword(cd_ea + 16), -1, ASCSTR_C);
    MakeName(c_ea, (is_meta ? "_OBJC_METACLASS_$_" : "_OBJC_CLASS_$_") + cl_name);
    MakeName(cd_ea, cl_name + (is_meta ? "_$metaData" : "_$classData"));
    
    // methods
    methodize(Dword(cd_ea + 20), is_meta, cl_name);
    
    // ivars
    i_ea = Dword(cd_ea + 28);
    if (i_ea > 0) {
        i_size = Dword(i_ea);
        i_count = Dword(i_ea + 4);
        MakeStruct(i_ea, "ivar_list_t");
        MakeName(i_ea, cl_name + (is_meta ? "_$classIvars" : "_$ivars"));
        i_ea = i_ea + 8;
        for (i_i = 0; i_i < i_count; i_i = i_i + 1) {
            MakeStruct(i_ea, "ivar_t");
            i_sname = GetString(Dword(i_ea+4), -1, ASCSTR_C);
            MakeDword(Dword(i_ea));
            MakeName(Dword(i_ea), "_OBJC_IVAR_$_" + cl_name + "." + i_sname);
            i_ea = i_ea + i_size;
        }
    }
}

/* categorize: Make 'c_ea' a category_t and analyze its methods. */
static categorize(c_ea) {
    auto cat_name, cl_name, s_name;
    cat_name = GetString(Dword(c_ea), -1, ASCSTR_C);
    s_name = substr(Name(Dword(c_ea + 4)), 14, -1);
    cl_name = s_name + "(" + cat_name + ")";
    methodize(Dword(c_ea + 8), 0, cl_name);
    methodize(Dword(c_ea + 12), 1, cl_name);
}

/* create_structs: Create the structures. */
static create_structs () {
    auto id;
    
    id = AddStruc(-1, "method_t");
    if (id != -1) {
        AddStrucMember(id, "name",  0, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "types", 4, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "imp",   8, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
    }
    
    id = AddStruc(-1, "method_list_t");
    if (id != -1) {
        AddStrucMember(id, "entsize_NEVER_USE",  0, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "count",              4, FF_DWRD|FF_DATA, -1, 4);
    }
    
    id = AddStruc(-1, "class_t");
    if (id != -1) {
        AddStrucMember(id, "isa",        0, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "superclass", 4, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "cache",      8, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "vtable",    12, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "data",      16, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
    }
    
    id = AddStruc(-1, "class_ro_t");
    if (id != -1) {
        AddStrucMember(id, "flags",           0, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "instanceStart",   4, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "instanceSize",    8, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "ivarLayout",     12, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "name",           16, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "baseMethods",    20, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "baseProtocols",  24, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "ivars",          28, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "weakIvarLayout", 32, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "baseProperties", 36, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
    }
    
    id = AddStruc(-1, "ivar_list_t");
    if (id != -1) {
        AddStrucMember(id, "entsize", 0, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "count",   4, FF_DWRD|FF_DATA, -1, 4);
    }
    
    id = AddStruc(-1, "ivar_t");
    if (id != -1) {
        AddStrucMember(id, "offset",     0, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "name",       4, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "type",       8, FF_DWRD|FF_0OFF|FF_1OFF|FF_DATA, -1, 4, -1, 0, REF_OFF32);
        AddStrucMember(id, "alignment", 12, FF_DWRD|FF_DATA, -1, 4);
        AddStrucMember(id, "size",      16, FF_DWRD|FF_DATA, -1, 4);
    }
}

static main () {
    auto cl_ea, cl_base, c_ea, s_base, s_ea, s_name, cr_base, cr_ea, cr_target;
    auto cat_base, cat_ea;
    auto sr_base, sr_ea, sr_target;
    
    if (GetLongPrm(INF_FILETYPE) != 25 || (GetLongPrm(INF_PROCNAME) & 0xffffff) != 0x4d5241) {
        Warning("fixobjc2.idc only works for Mach-O binaries with ARM processors.");
        return;
    }

    create_structs();

    offsetize("__objc_classrefs");
    offsetize("__objc_classlist");
    offsetize("__objc_catlist");
    offsetize("__objc_protolist");
    offsetize("__objc_superrefs");
    offsetize("__objc_selrefs");
    
    // name all selectors
    s_base = SegByBase(SegByName("__objc_selrefs"));
    if (s_base >= 0) {
        for (s_ea = SegStart(s_base); s_ea != SegEnd(s_base); s_ea = s_ea + 4) {
            s_name = GetString(Dword(s_ea), -1, ASCSTR_C);
            MakeRptCmt(s_ea, "@selector(" + s_name + ")");
        }
    }
    
    // find all methods & ivars.
    cl_base = SegByBase(SegByName("__objc_classlist"));
    if (cl_base >= 0) {
        for (cl_ea = SegStart(cl_base); cl_ea != SegEnd(cl_base); cl_ea = cl_ea + 4) {
            c_ea = Dword(cl_ea);
            classize(c_ea, 0);
            classize(Dword(c_ea), 1);
        }
    }
        
    // name all classrefs
    cr_base = SegByBase(SegByName("__objc_classrefs"));
    if (cr_base >= 0) {
        for (cr_ea = SegStart(cr_base); cr_ea != SegEnd(cr_base); cr_ea = cr_ea + 4) {
            cr_target = Dword(cr_ea);
            if (cr_target > 0) {
                MakeRptCmt(cr_ea, Name(cr_target));
            }
        }
    }
    
    // name all superrefs
    sr_base = SegByBase(SegByName("__objc_superrefs"));
    if (sr_base >= 0) {
       for (sr_ea = SegStart(sr_base); sr_ea != SegEnd(sr_base); sr_ea = sr_ea + 4) {
           sr_target = Dword(sr_ea);
           if (sr_target > 0) {
               MakeRptCmt(sr_ea, Name(sr_target));
           }
       }
    }
    
    // categories.
    cat_base = SegByBase(SegByName("__objc_catlist"));
    if (cat_base >= 0) {
        for (cat_ea = SegStart(cat_base); cat_ea != SegEnd(cat_base); cat_ea = cat_ea + 4) {
            categorize(Dword(cat_ea));
        }
    } 
}
