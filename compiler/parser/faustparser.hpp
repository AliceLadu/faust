/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_PARSER_FAUSTPARSER_HPP_INCLUDED
# define YY_YY_PARSER_FAUSTPARSER_HPP_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    WITH = 258,
    SPLIT = 259,
    MIX = 260,
    SEQ = 261,
    PAR = 262,
    REC = 263,
    LT = 264,
    LE = 265,
    EQ = 266,
    GT = 267,
    GE = 268,
    NE = 269,
    ADD = 270,
    SUB = 271,
    OR = 272,
    MUL = 273,
    DIV = 274,
    MOD = 275,
    AND = 276,
    XOR = 277,
    LSH = 278,
    RSH = 279,
    POWOP = 280,
    FDELAY = 281,
    DELAY1 = 282,
    APPL = 283,
    DOT = 284,
    MEM = 285,
    PREFIX = 286,
    INTCAST = 287,
    FLOATCAST = 288,
    FFUNCTION = 289,
    FCONSTANT = 290,
    FVARIABLE = 291,
    BUTTON = 292,
    CHECKBOX = 293,
    VSLIDER = 294,
    HSLIDER = 295,
    NENTRY = 296,
    VGROUP = 297,
    HGROUP = 298,
    TGROUP = 299,
    HBARGRAPH = 300,
    VBARGRAPH = 301,
    ATTACH = 302,
    ACOS = 303,
    ASIN = 304,
    ATAN = 305,
    ATAN2 = 306,
    COS = 307,
    SIN = 308,
    TAN = 309,
    EXP = 310,
    LOG = 311,
    LOG10 = 312,
    POWFUN = 313,
    SQRT = 314,
    ABS = 315,
    MIN = 316,
    MAX = 317,
    FMOD = 318,
    REMAINDER = 319,
    FLOOR = 320,
    CEIL = 321,
    RINT = 322,
    RDTBL = 323,
    RWTBL = 324,
    SELECT2 = 325,
    SELECT3 = 326,
    INT = 327,
    FLOAT = 328,
    LAMBDA = 329,
    WIRE = 330,
    CUT = 331,
    ENDDEF = 332,
    VIRG = 333,
    LPAR = 334,
    RPAR = 335,
    LBRAQ = 336,
    RBRAQ = 337,
    LCROC = 338,
    RCROC = 339,
    DEF = 340,
    IMPORT = 341,
    COMPONENT = 342,
    LIBRARY = 343,
    ENVIRONMENT = 344,
    WAVEFORM = 345,
    IPAR = 346,
    ISEQ = 347,
    ISUM = 348,
    IPROD = 349,
    INPUTS = 350,
    OUTPUTS = 351,
    STRING = 352,
    FSTRING = 353,
    IDENT = 354,
    EXTRA = 355,
    DECLARE = 356,
    CASE = 357,
    ARROW = 358,
    BDOC = 359,
    EDOC = 360,
    BEQN = 361,
    EEQN = 362,
    BDGM = 363,
    EDGM = 364,
    BLST = 365,
    ELST = 366,
    BMETADATA = 367,
    EMETADATA = 368,
    DOCCHAR = 369,
    NOTICE = 370,
    LISTING = 371,
    LSTTRUE = 372,
    LSTFALSE = 373,
    LSTDEPENDENCIES = 374,
    LSTMDOCTAGS = 375,
    LSTDISTRIBUTED = 376,
    LSTEQ = 377,
    LSTQ = 378
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 78 "parser/faustparser.y" /* yacc.c:1909  */

	CTree* 	exp;
	char* str;
	string* cppstr;
	bool b;

#line 185 "parser/faustparser.hpp" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_PARSER_FAUSTPARSER_HPP_INCLUDED  */
