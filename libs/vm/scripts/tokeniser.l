%option noyywrap nodefault reentrant bison-bridge
%option outfile="../src/tokeniser.cpp"
%option header-file="../include/vm/tokeniser.hpp"
%x COMMENT

%{

#include "vm/token.hpp"
#define YY_EXTRA_TYPE fetch::vm::Location*
#define YYSTYPE fetch::vm::Token

#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#else
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif



int Set(fetch::vm::Token *token, const fetch::vm::Token::Kind kind, char const *text,
	uint16_t const length, fetch::vm::Location *location)
{
	//printf("kind=%d text=[%s] length=%d line=%d offset=%d\n",
		//kind, text, length, location->line, location->offset);
	token->kind = kind;
	token->offset = location->offset;
	token->line = location->line;
	token->length = length;
	token->text = text;
	location->offset += length;
	return 1;
}

%}


EXP			[Ee][+-]?[0-9]+
STR1			([^"\\\n]|\\.)*
STR2			([^'\\\n]|\\.)*
INTEGER		[0-9][0-9]*
REAL1		([0-9]*\.[0-9]+|[0-9]+\.){EXP}?
REAL2		[0-9]+{EXP}
FIXED1		([0-9]*\.[0-9]+|[0-9]+\.)
FIXED2		[0-9]
IDENT		[_a-zA-Z][_a-zA-Z0-9]*

%%

	/* whitespace */
[ \t\r]+ {
	yyextra->offset += yyleng;
}
[\n] {
	yyextra->offset += yyleng;
	yyextra->line++;
}

	/* comments */
"/*" {
	yyextra->offset += yyleng;
	BEGIN(COMMENT);
}
<COMMENT>\n {
	yyextra->offset += yyleng;
	yyextra->line++;
}
<COMMENT>[^*\n]+ {
	yyextra->offset += yyleng;
}
<COMMENT>\**[^/\n] {
	yyextra->offset += yyleng;
}
<COMMENT>\**"*/" {
	yyextra->offset += yyleng;
	BEGIN(INITIAL);
}
"//".*\n {
	yyextra->offset += yyleng;
	yyextra->line++;
}

	/* numbers */

{INTEGER}i8 {
	return Set(yylval, fetch::vm::Token::Kind::Integer8, yytext, yyleng, yyextra);
}

{INTEGER}u8 {
	return Set(yylval, fetch::vm::Token::Kind::UnsignedInteger8, yytext, yyleng, yyextra);
}

{INTEGER}i16 {
	return Set(yylval, fetch::vm::Token::Kind::Integer16, yytext, yyleng, yyextra);
}

{INTEGER}u16 {
	return Set(yylval, fetch::vm::Token::Kind::UnsignedInteger16, yytext, yyleng, yyextra);
}

{INTEGER}i32 {
	return Set(yylval, fetch::vm::Token::Kind::Integer32, yytext, yyleng, yyextra);
}

{INTEGER}u32 {
	return Set(yylval, fetch::vm::Token::Kind::UnsignedInteger32, yytext, yyleng, yyextra);
}

{INTEGER}i64 {
	return Set(yylval, fetch::vm::Token::Kind::Integer64, yytext, yyleng, yyextra);
}

{INTEGER}u64 {
	return Set(yylval, fetch::vm::Token::Kind::UnsignedInteger64, yytext, yyleng, yyextra);
}

{INTEGER} {
	return Set(yylval, fetch::vm::Token::Kind::Integer32, yytext, yyleng, yyextra);
}

{REAL1}f |
{REAL2}f {
	return Set(yylval, fetch::vm::Token::Kind::Float32, yytext, yyleng, yyextra);
}

{REAL1} |
{REAL2} {
	return Set(yylval, fetch::vm::Token::Kind::Float64, yytext, yyleng, yyextra);
}

{FIXED1}fp32 |
{FIXED2}fp32 {
	return Set(yylval, fetch::vm::Token::Kind::Fixed32, yytext, yyleng, yyextra);
}

{FIXED1}fp64 |
{FIXED2}fp64 {
	return Set(yylval, fetch::vm::Token::Kind::Fixed64, yytext, yyleng, yyextra);
}

	/* strings */
\"{STR1}\" |
\'{STR2}\' {
	return Set(yylval, fetch::vm::Token::Kind::String, yytext, yyleng, yyextra);
}

\"{STR1}\n |
\'{STR2}\n {
	Set(yylval, fetch::vm::Token::Kind::BadString, yytext, yyleng, yyextra);
	yyextra->line++;
	return 1;
}

	/* keywords */
"true"        {return Set(yylval, fetch::vm::Token::Kind::True, yytext, yyleng, yyextra);}
"false"       {return Set(yylval, fetch::vm::Token::Kind::False, yytext, yyleng, yyextra);}
"null"        {return Set(yylval, fetch::vm::Token::Kind::Null, yytext, yyleng, yyextra);}
"function"    {return Set(yylval, fetch::vm::Token::Kind::Function, yytext, yyleng, yyextra);}
"endfunction" {return Set(yylval, fetch::vm::Token::Kind::EndFunction, yytext, yyleng, yyextra);}
"while"       {return Set(yylval, fetch::vm::Token::Kind::While, yytext, yyleng, yyextra);}
"endwhile"    {return Set(yylval, fetch::vm::Token::Kind::EndWhile, yytext, yyleng, yyextra);}
"for"         {return Set(yylval, fetch::vm::Token::Kind::For, yytext, yyleng, yyextra);}
"in"          {return Set(yylval, fetch::vm::Token::Kind::In, yytext, yyleng, yyextra);}
"endfor"      {return Set(yylval, fetch::vm::Token::Kind::EndFor, yytext, yyleng, yyextra);}
"if"          {return Set(yylval, fetch::vm::Token::Kind::If, yytext, yyleng, yyextra);}
"elseif"      {return Set(yylval, fetch::vm::Token::Kind::ElseIf, yytext, yyleng, yyextra);}
"else"        {return Set(yylval, fetch::vm::Token::Kind::Else, yytext, yyleng, yyextra);}
"endif"       {return Set(yylval, fetch::vm::Token::Kind::EndIf, yytext, yyleng, yyextra);}
"var"         {return Set(yylval, fetch::vm::Token::Kind::Var, yytext, yyleng, yyextra);}
"return"      {return Set(yylval, fetch::vm::Token::Kind::Return, yytext, yyleng, yyextra);}
"break"       {return Set(yylval, fetch::vm::Token::Kind::Break, yytext, yyleng, yyextra);}
"continue"    {return Set(yylval, fetch::vm::Token::Kind::Continue, yytext, yyleng, yyextra);}
"persistent"  {return Set(yylval, fetch::vm::Token::Kind::Persistent, yytext, yyleng, yyextra);}
"use"         {return Set(yylval, fetch::vm::Token::Kind::Use, yytext, yyleng, yyextra);}
"as"          {return Set(yylval, fetch::vm::Token::Kind::As, yytext, yyleng, yyextra);}


	/* identifiers */
{IDENT} {
	return Set(yylval, fetch::vm::Token::Kind::Identifier, yytext, yyleng, yyextra);
}

	/* annotation identifiers */
@{IDENT} {
	return Set(yylval, fetch::vm::Token::Kind::AnnotationIdentifier, yytext, yyleng, yyextra);
}

	/*  punctuation and operators */
","   {return Set(yylval, fetch::vm::Token::Kind::Comma, yytext, yyleng, yyextra);}
"."   {return Set(yylval, fetch::vm::Token::Kind::Dot, yytext, yyleng, yyextra);}
":"   {return Set(yylval, fetch::vm::Token::Kind::Colon, yytext, yyleng, yyextra);}
";"   {return Set(yylval, fetch::vm::Token::Kind::SemiColon, yytext, yyleng, yyextra);}
"("   {return Set(yylval, fetch::vm::Token::Kind::LeftParenthesis, yytext, yyleng, yyextra);}
")"   {return Set(yylval, fetch::vm::Token::Kind::RightParenthesis, yytext, yyleng, yyextra);}
"["   {return Set(yylval, fetch::vm::Token::Kind::LeftSquareBracket, yytext, yyleng, yyextra);}
"]"   {return Set(yylval, fetch::vm::Token::Kind::RightSquareBracket, yytext, yyleng, yyextra);}
"{"   {return Set(yylval, fetch::vm::Token::Kind::LeftBrace, yytext, yyleng, yyextra);}
"}"   {return Set(yylval, fetch::vm::Token::Kind::RightBrace, yytext, yyleng, yyextra);}
"%"   {return Set(yylval, fetch::vm::Token::Kind::Modulo, yytext, yyleng, yyextra);}
"+"   {return Set(yylval, fetch::vm::Token::Kind::Plus, yytext, yyleng, yyextra);}
"-"   {return Set(yylval, fetch::vm::Token::Kind::Minus, yytext, yyleng, yyextra);}
"*"   {return Set(yylval, fetch::vm::Token::Kind::Multiply, yytext, yyleng, yyextra);}
"/"   {return Set(yylval, fetch::vm::Token::Kind::Divide, yytext, yyleng, yyextra);}
"%="  {return Set(yylval, fetch::vm::Token::Kind::InplaceModulo, yytext, yyleng, yyextra);}
"+="  {return Set(yylval, fetch::vm::Token::Kind::InplaceAdd, yytext, yyleng, yyextra);}
"-="  {return Set(yylval, fetch::vm::Token::Kind::InplaceSubtract, yytext, yyleng, yyextra);}
"*="  {return Set(yylval, fetch::vm::Token::Kind::InplaceMultiply, yytext, yyleng, yyextra);}
"/="  {return Set(yylval, fetch::vm::Token::Kind::InplaceDivide, yytext, yyleng, yyextra);}
"="   {return Set(yylval, fetch::vm::Token::Kind::Assign, yytext, yyleng, yyextra);}
"=="  {return Set(yylval, fetch::vm::Token::Kind::Equal, yytext, yyleng, yyextra);}
"!="  {return Set(yylval, fetch::vm::Token::Kind::NotEqual, yytext, yyleng, yyextra);}
"<"   {return Set(yylval, fetch::vm::Token::Kind::LessThan, yytext, yyleng, yyextra);}
"<="  {return Set(yylval, fetch::vm::Token::Kind::LessThanOrEqual, yytext, yyleng, yyextra);}
">"   {return Set(yylval, fetch::vm::Token::Kind::GreaterThan, yytext, yyleng, yyextra);}
">="  {return Set(yylval, fetch::vm::Token::Kind::GreaterThanOrEqual, yytext, yyleng, yyextra);}
"&&"  {return Set(yylval, fetch::vm::Token::Kind::And, yytext, yyleng, yyextra);}
"||"  {return Set(yylval, fetch::vm::Token::Kind::Or, yytext, yyleng, yyextra);}
"!"   {return Set(yylval, fetch::vm::Token::Kind::Not, yytext, yyleng, yyextra);}
"++"  {return Set(yylval, fetch::vm::Token::Kind::Inc, yytext, yyleng, yyextra);}
"--"  {return Set(yylval, fetch::vm::Token::Kind::Dec, yytext, yyleng, yyextra);}

<<EOF>> {Set(yylval, fetch::vm::Token::Kind::EndOfInput, "", 0, yyextra); return 0;}

	/* catch-all for unrecognised */
. {return Set(yylval, fetch::vm::Token::Kind::Unknown, yytext, yyleng, yyextra);}

%%
