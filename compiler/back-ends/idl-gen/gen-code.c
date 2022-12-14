/*
 *   compiler/back_ends/idl_gen/gen_idl_code.c - routines for printing CORBA IDL code from type trees
 *
 *   assumes that the type tree has already been run through the
 *   IDL type generator (idl_gen/types.c).
 *
 * Mike Sample
 * 92
 * Copyright (C) 1991, 1992 Michael Sample
 *           and the University of British Columbia
 *
 * Copyright ? 1995 Robert Joop
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * $Header: /usr/app/odstb/CVS/snacc/compiler/back-ends/idl-gen/gen-code.c,v 1.2 1997/03/13 09:15:21 wan Exp $
 * $Log: gen-code.c,v $
 * Revision 1.2  1997/03/13 09:15:21  wan
 * Improved dependency generation for stupid makedepends.
 * Corrected PeekTag to peek into buffer only as far as necessary.
 * Added installable error handler.
 * Fixed small glitch in idl-code generator (Markku Savela <msa@msa.tte.vtt.fi>).
 *
 * Revision 1.1  1997/01/01 20:25:34  rj
 * first draft
 *
 */

#include "snacc.h"

#if STDC_HEADERS || HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <stdio.h>

#include "asn-incl.h"
#include "asn1module.h"
#include "define.h"
#include "mem.h"
#include "lib-types.h"
#include "rules.h"
#include "types.h"
#include "cond.h"
#include "str-util.h"
#include "snacc-util.h"
#include "print.h"
#include "tag-util.h"  /* get GetTags/FreeTags/CountTags/TagByteLen */
#include "gen-vals.h"
#include "gen-any.h"
#include "gen-code.h"


static long int		longJmpValG  = -100;

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintComment PARAMS ((idl, m),
    FILE *idl _AND_
    Module *m)
{
    long int t;

    t = time (0);
    fprintf (idl, "//   NOTE: this is a machine generated file -- editing not recommended\n");
    fprintf (idl, "//\n");
    fprintf (idl, "// %s -- IDL for ASN.1 module %s\n", m->idlFileName, m->modId->name);
    fprintf (idl, "//\n");
    fprintf (idl, "//   This file was generated by snacc on %s", ctime (&t));
    fprintf (idl, "//   UBC snacc written by Mike Sample\n");
    fprintf (idl, "//   IDL generator written by Robert Joop\n");
    fprintf (idl, "\n");

} /* PrintComment */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIncludes PARAMS ((idl, mods, m),
    FILE	*idl _AND_
    ModuleList	*mods _AND_
    Module	*m)
{
    void	*tmp;
    Module	*currMod;

    fprintf (idl, "#include \"ASN1Types.idl\"\n");
    fprintf (idl, "#include \"BitString.idl\"\n");

    tmp = (void *)CURR_LIST_NODE (mods); /* remember curr loc */
    FOR_EACH_LIST_ELMT (currMod, mods)
        fprintf (idl, "#include \"%s\"\n", currMod->idlFileName);
    SET_CURR_LIST_NODE (mods, tmp);

} /* PrintIncludes */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintTypeDecl PARAMS ((f, td),
    FILE	*f _AND_
    TypeDef	*td)
{
    switch (td->type->basicType->choiceId)
    {
        case BASICTYPE_COMPONENTSOF:
        case BASICTYPE_SELECTION:
        case BASICTYPE_UNKNOWN:
        case BASICTYPE_MACRODEF:
        case BASICTYPE_MACROTYPE:
            return; /* do nothing */

        case BASICTYPE_ENUMERATED:
            if (IsNewType (td->type))
                fprintf (f, "  enum %s;\n", td->idlTypeDefInfo->typeName);
	    break;

        default:
            if (IsNewType (td->type))
                fprintf (f, "  struct %s;\n", td->idlTypeDefInfo->typeName);
    }

} /* PrintTypeDecl */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIDLTypeAndName PARAMS ((idl, mods, m, r, td, parent, t),
    FILE	*idl _AND_
    ModuleList	*mods _AND_
    Module	*m _AND_
    IDLRules	*r _AND_
    TypeDef	*td _AND_
    Type	*parent _AND_
    Type	*t)
{
    if (t->optional)
	fprintf (idl, "union %sOptional switch (boolean) { case True: %s %s; };\n", t->idlTypeRefInfo->typeName, t->idlTypeRefInfo->typeName, t->idlTypeRefInfo->fieldName);
    else
	fprintf (idl, "%s	%s;\n", t->idlTypeRefInfo->typeName, t->idlTypeRefInfo->fieldName);

#if 0
    if (t->idlTypeRefInfo->isPtr)
        fprintf (idl, "*");
#endif

} /* PrintIDLTypeAndName */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
/*
 * prints typedef or new class given an ASN.1  type def of a primitive type
 * or typeref.  Uses inheritance to cover re-tagging and named elmts.
 */
static void
PrintIDLSimpleDef PARAMS ((idl, r, td),
    FILE *idl _AND_
    IDLRules *r _AND_
    TypeDef *td)
{
    int	hasNamedElmts;
    CNamedElmt *n;

    fprintf (idl, "  /* ");
    SpecialPrintType (idl, td, td->type);
    fprintf (idl, " */\n");

    if (hasNamedElmts = HasNamedElmts (td->type))
    {
	int	tlen = strlen (td->idlTypeDefInfo->typeName) - strlen (r->typeSuffix);
	switch (GetBuiltinType (td->type))
	{
	    case BASICTYPE_INTEGER:
		fprintf (idl, "  typedef %s %s;\n", td->type->idlTypeRefInfo->typeName, td->idlTypeDefInfo->typeName);
		FOR_EACH_LIST_ELMT (n, td->type->idlTypeRefInfo->namedElmts)
		    fprintf (idl, "  const %s %.*s_%s = %d;\n", td->idlTypeDefInfo->typeName, tlen, td->idlTypeDefInfo->typeName, n->name, n->value);
		break;
	    case BASICTYPE_ENUMERATED:
		fprintf (idl, "  enum %s\n", td->idlTypeDefInfo->typeName);
		fprintf (idl, "  {\n");
		FOR_EACH_LIST_ELMT (n, td->type->idlTypeRefInfo->namedElmts)
		{
		    char comma = (n != (CNamedElmt *)LAST_LIST_ELMT (td->type->idlTypeRefInfo->namedElmts)) ? ',' : ' ';
		    fprintf (idl, "    %s%c	// (original value = %d)\n", n->name, comma, n->value);
		}
		fprintf (idl, "  };\n");
		break;
	    case BASICTYPE_BITSTRING:
		fprintf (idl, "  typedef %s %s;\n", td->type->idlTypeRefInfo->typeName, td->idlTypeDefInfo->typeName);
		FOR_EACH_LIST_ELMT (n, td->type->idlTypeRefInfo->namedElmts)
		    fprintf (idl, "  const unsigned long %.*s_%s = %d;\n", tlen, td->idlTypeDefInfo->typeName, n->name, n->value);
		break;
	    default:
		fprintf (idl, "  ???!\n");
	}
    }
    else
	fprintf (idl, "  typedef %s %s;\n\n", td->type->idlTypeRefInfo->typeName, td->idlTypeDefInfo->typeName);

} /* PrintIDLSimpleDef */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIDLChoiceDefCode PARAMS ((idl, mods, m, r, td, parent, choice),
    FILE	*idl _AND_
    ModuleList	*mods _AND_
    Module	*m _AND_
    IDLRules	*r _AND_
    TypeDef	*td _AND_
    Type	*parent _AND_
    Type	*choice)
{
    NamedType	*e;

    /* put class spec in idl file */

    /* write out choiceId enum type */

    fprintf (idl, "  enum %s%s\n", td->idlTypeDefInfo->typeName, r->choiceEnumSuffix);
    fprintf (idl, "  {\n");
    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
    {
        fprintf (idl, "    %s", e->type->idlTypeRefInfo->choiceIdSymbol);
        if (e != (NamedType *)LAST_LIST_ELMT (choice->basicType->a.choice))
            fprintf (idl, ",\n");
        else
            fprintf (idl, "\n");
    }
    fprintf (idl, "  };\n\n");

    /* write out the choice element anonymous union */
    fprintf (idl, "  union %s switch (%s%s)\n", td->idlTypeDefInfo->typeName, td->idlTypeDefInfo->typeName, r->choiceEnumSuffix);
    fprintf (idl, "  {\n");
    FOR_EACH_LIST_ELMT (e, choice->basicType->a.choice)
    {
#if 0
        fprintf (idl, "    case %s:	%s %s;\n", e->type->idlTypeRefInfo->choiceIdSymbol, e->type->idlTypeRefInfo->typeName, e->type->idlTypeRefInfo->fieldName);
#else
        fprintf (idl, "    case %s:	", e->type->idlTypeRefInfo->choiceIdSymbol);
	PrintIDLTypeAndName (idl, mods, m, r, td, choice, e->type);
#endif
    }
    fprintf (idl, "  };\n\n");

} /* PrintIDLChoiceDefCode */


/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIDLSeqDefCode PARAMS ((idl, mods, m, r, td, parent, seq),
    FILE *idl _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    IDLRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *seq)
{
    NamedType *e;

    /* put class spec in idl file */

    fprintf (idl, "  struct %s\n", td->idlTypeDefInfo->typeName);
    fprintf (idl, "  {\n");

    /* write out the sequence elmts */
    FOR_EACH_LIST_ELMT (e, seq->basicType->a.sequence)
    {
        fprintf (idl, "    ");
	PrintIDLTypeAndName (idl, mods, m, r, td, seq, e->type);
    }

    /* close struct definition */
    fprintf (idl, "  };\n\n\n");

} /* PrintIDLSeqDefCode */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIDLSetDefCode PARAMS ((idl, mods, m, r, td, parent, set),
    FILE *idl _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    IDLRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *set)
{
    NamedType *e;

    /* put class spec in idl file */

    fprintf (idl, "  struct %s\n", td->idlTypeDefInfo->typeName);
    fprintf (idl, "  {\n");

    /* write out the set elmts */
    FOR_EACH_LIST_ELMT (e, set->basicType->a.set)
    {
        fprintf (idl, "    ");
        PrintIDLTypeAndName (idl, mods, m, r, td, set, e->type);
    }

    fprintf (idl, "  };\n\n");

} /* PrintIDLSetDefCode */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintCxxSetOfDefCode PARAMS ((idl, mods, m, r, td, parent, setOf),
    FILE *idl _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    IDLRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *setOf)
{
    char *lcn; /* list class name */
    char *ecn; /* (list) elmt class name */

    lcn = td->idlTypeDefInfo->typeName;
    ecn = setOf->basicType->a.setOf->idlTypeRefInfo->typeName;
    fprintf (idl, "  typedef sequence<%s> %s;\n", ecn, lcn);

} /* PrintCxxSetOfDefCode */


/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintCxxAnyDefCode PARAMS ((idl, mods, m, r, td, parent, any),
    FILE *idl _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    IDLRules *r _AND_
    TypeDef *td _AND_
    Type *parent _AND_
    Type *any)
{
    fprintf (idl, "  /* ");
    SpecialPrintType (idl, td, td->type);
    fprintf (idl, " */\n");
    fprintf (idl, "  typedef %s %s;\n\n", td->type->idlTypeRefInfo->typeName, td->idlTypeDefInfo->typeName);
} /* PrintCxxAnyDefCode */


/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
static void
PrintIDLTypeDefCode PARAMS ((idl, mods, m, r, td),
    FILE *idl _AND_
    ModuleList *mods _AND_
    Module *m _AND_
    IDLRules *r _AND_
    TypeDef *td)
{
    switch (td->type->basicType->choiceId)
    {
        case BASICTYPE_BOOLEAN:  /* library type */
        case BASICTYPE_REAL:  /* library type */
        case BASICTYPE_OCTETSTRING:  /* library type */
        case BASICTYPE_NULL:  /* library type */
        case BASICTYPE_OID:  /* library type */
        case BASICTYPE_INTEGER:  /* library type */
        case BASICTYPE_BITSTRING:  /* library type */
        case BASICTYPE_ENUMERATED:  /* library type */
            PrintIDLSimpleDef (idl, r, td);
            break;

        case BASICTYPE_SEQUENCEOF:  /* list types */
        case BASICTYPE_SETOF:
            PrintCxxSetOfDefCode (idl, mods, m, r, td, NULL, td->type);
            break;

        case BASICTYPE_IMPORTTYPEREF:  /* type references */
        case BASICTYPE_LOCALTYPEREF:
            /*
             * if this type has been re-tagged then
             * must create new class instead of using a typedef
             */
            PrintIDLSimpleDef (idl, r, td);
            break;

        case BASICTYPE_ANYDEFINEDBY:  /* ANY types */
        case BASICTYPE_ANY:
/*
            fprintf (stderr, "  ANY types require modification. ");
            fprintf (stderr, "  The source files will have a \" ANY - Fix Me! \" comment before related code.\n\n");
*/
            PrintCxxAnyDefCode (idl, mods, m, r, td, NULL, td->type);
            break;

        case BASICTYPE_CHOICE:
            PrintIDLChoiceDefCode (idl, mods, m, r, td, NULL, td->type);
            break;

        case BASICTYPE_SET:
            PrintIDLSetDefCode (idl, mods, m, r, td, NULL, td->type);
            break;

        case BASICTYPE_SEQUENCE:
            PrintIDLSeqDefCode (idl, mods, m, r, td, NULL, td->type);
            break;

        case BASICTYPE_COMPONENTSOF:
        case BASICTYPE_SELECTION:
        case BASICTYPE_UNKNOWN:
        case BASICTYPE_MACRODEF:
        case BASICTYPE_MACROTYPE:
            /* do nothing */
            break;
    }
} /* PrintIDLTypeDefCode */

/*\[sep]--------------------------------------------------------------------------------------------------------------------------*/
void
PrintIDLCode PARAMS ((idl, mods, m, r, longJmpVal),
    FILE	*idl _AND_
    ModuleList	*mods _AND_
    Module	*m _AND_
    IDLRules	*r _AND_
    long int	longJmpVal _AND_
    int		printValues)
{
    TypeDef	*td;
    ValueDef	*vd;

    longJmpValG = longJmpVal;

    PrintComment (idl, m);

    PrintConditionalIncludeOpen (idl, m->idlFileName);

    PrintIncludes (idl, mods, m);

    fprintf (idl, "\n");
    fprintf (idl, "module %s\n{\n\n", m->idlname);

    fprintf (idl, "  //----------------------------------------------------------------------------\n");
    fprintf (idl, "  // type declarations:\n\n");
    FOR_EACH_LIST_ELMT (td, m->typeDefs)
        PrintTypeDecl (idl, td);
    fprintf (idl, "\n");

    if (printValues)
    {
	fprintf (idl, "  //----------------------------------------------------------------------------\n");
        fprintf (idl, "  // value definitions:\n\n");
        FOR_EACH_LIST_ELMT (vd, m->valueDefs)
            PrintIDLValueDef (idl, r, vd);
        fprintf (idl, "\n");
    }

    fprintf (idl, "  //----------------------------------------------------------------------------\n");
    fprintf (idl, "  // type definitions:\n\n");

#if 0
    PrintIDLAnyCode (idl, r, mods, m);
#endif

    FOR_EACH_LIST_ELMT (td, m->typeDefs)
    {
        PrintIDLTypeDefCode (idl, mods, m, r, td);
	fputc ('\n', idl);
    }

    fprintf (idl, "}; // end of module %s\n", m->idlname);

    PrintConditionalIncludeClose (idl, m->idlFileName);

} /* PrintIDLCode */

/*\[banner "EOF"]-----------------------------------------------------------------------------------------------------------------*/
