//------------------------------------------------------------------------------
//
//   Copyright 2018-2019 Fetch.AI Limited
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
//
//------------------------------------------------------------------------------

#include "meta/value_util.hpp"
#include "vm/parser.hpp"

// Must define YYSTYPE and YY_EXTRA_TYPE *before* including the flex-generated header
#define YYSTYPE fetch::vm::Token
#define YY_EXTRA_TYPE fetch::vm::Location *
#include "vm/tokeniser.hpp"

#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace fetch {
namespace vm {

Parser::Parser()
  : template_names_{"Matrix", "Array", "Map", "State"}
{}

BlockNodePtr Parser::Parse(std::string const &filename, std::string const &source,
                           std::vector<std::string> &errors)
{
  Tokenise(source);

  index_ = -1;
  token_ = nullptr;
  errors_.clear();
  blocks_.clear();
  groups_.clear();
  operators_.clear();
  rpn_.clear();
  infix_stack_.clear();

  BlockNodePtr root      = CreateBlockNode(NodeKind::Root, "", 0);
  BlockNodePtr file_node = CreateBlockNode(NodeKind::File, filename, 1);
  root->block_children.push_back(file_node);
  ParseBlock(*file_node);
  errors        = std::move(errors_);

  tokens_.clear();
  blocks_.clear();
  groups_.clear();
  operators_.clear();
  rpn_.clear();
  infix_stack_.clear();

  if (errors.empty())
  {
    return root;
  }
  return {};
}

void Parser::Tokenise(std::string const &source)
{
  tokens_.clear();
  Location location;
  yyscan_t scanner;
  yylex_init_extra(&location, &scanner);
  YY_BUFFER_STATE bp = yy_scan_string(source.data(), scanner);
  yy_switch_to_buffer(bp, scanner);
  // There is always at least one token: the last is EndOfInput
  Token token;
  do
  {
    yylex(&token, scanner);
    tokens_.push_back(token);
  } while (token.kind != Token::Kind::EndOfInput);
  yy_delete_buffer(bp, scanner);
  yylex_destroy(scanner);
}

bool Parser::ParseBlock(BlockNode &node)
{
  blocks_.push_back(node.node_kind);
  do
  {
    bool    quit  = false;
    bool    state = false;
    NodePtr child;
    Next();
    switch (token_->kind)
    {
    case Token::Kind::AnnotationIdentifier:
    case Token::Kind::Function:
    {
      child = ParseFunctionDefinition();
      break;
    }
    case Token::Kind::EndFunction:
    {
      if (node.node_kind == NodeKind::FunctionDefinitionStatement)
      {
        quit  = true;
        state = true;
        break;
      }
      AddError("no matching 'function'");
      break;
    }
    case Token::Kind::While:
    {
      child = ParseWhileStatement();
      break;
    }
    case Token::Kind::EndWhile:
    {
      if (node.node_kind == NodeKind::WhileStatement)
      {
        quit  = true;
        state = true;
        break;
      }
      AddError("no matching 'while'");
      break;
    }
    case Token::Kind::For:
    {
      child = ParseForStatement();
      break;
    }
    case Token::Kind::EndFor:
    {
      if (node.node_kind == NodeKind::ForStatement)
      {
        quit  = true;
        state = true;
        break;
      }
      AddError("no matching 'for'");
      break;
    }
    case Token::Kind::If:
    {
      child = ParseIfStatement();
      break;
    }
    case Token::Kind::ElseIf:
    case Token::Kind::Else:
    {
      if ((node.node_kind == NodeKind::If) || (node.node_kind == NodeKind::ElseIf))
      {
        quit  = true;
        state = true;
        break;
      }
      AddError("no matching 'if' or 'elseif'");
      break;
    }
    case Token::Kind::EndIf:
    {
      if ((node.node_kind == NodeKind::If) || (node.node_kind == NodeKind::ElseIf) ||
          (node.node_kind == NodeKind::Else))
      {
        quit  = true;
        state = true;
        break;
      }
      AddError("no matching 'if', 'elseif' or 'else'");
      break;
    }
    case Token::Kind::Var:
    {
      child = ParseVarStatement();
      break;
    }
    case Token::Kind::Return:
    {
      child = ParseReturnStatement();
      break;
    }
    case Token::Kind::Break:
    {
      child = ParseBreakStatement();
      break;
    }
    case Token::Kind::Continue:
    {
      child = ParseContinueStatement();
      break;
    }
    case Token::Kind::EndOfInput:
    {
      quit = true;
      if (node.node_kind == NodeKind::File)
      {
        state = true;
      }
      else
      {
        AddError("expected statement or block terminator");
        state = false;
      }
      break;
    }
    default:
    {
      Undo();
      child = ParseExpressionStatement();
      break;
    }
    }  // switch
    if (quit)
    {
      // Store information on the block terminator
      node.block_terminator_text = token_->text;
      node.block_terminator_line = token_->line;
      blocks_.pop_back();
      return state;
    }
    if (child == nullptr)
    {
      // We got a syntax error but we want to keep on parsing to try to find
      // more syntax errors. Try to move to the start of the next
      // statement, ready to continue parsing from there.
      GoToNextStatement();
      continue;
    }
    node.block_children.push_back(std::move(child));
  } while (true);
}

BlockNodePtr Parser::ParseFunctionDefinition()
{
  NodePtr annotations_node;
  if (token_->kind == Token::Kind::AnnotationIdentifier)
  {
    annotations_node = ParseAnnotations();
    if (annotations_node)
    {
      if (token_->kind != Token::Kind::Function)
      {
        AddError("");
        annotations_node = nullptr;
      }
    }
    if (annotations_node == nullptr)
    {
      while ((token_->kind != Token::Kind::EndOfInput) && (token_->kind != Token::Kind::Function))
      {
        Next();
      }
      if (token_->kind == Token::Kind::EndOfInput)
      {
        return {};
      }
    }
  }
  BlockNodePtr function_definition_node =
      CreateBlockNode(NodeKind::FunctionDefinitionStatement, token_->text, token_->line);
  // NOTE: the annotations node is legitimately null if no annotations are supplied
  function_definition_node->children.push_back(annotations_node);
  bool ok = false;
  do
  {
    NodeKind const block_kind = blocks_.back();
    if (block_kind != NodeKind::File)
    {
      AddError("local function definitions are not permitted");
      break;
    }
    Next();
    if (token_->kind != Token::Kind::Identifier)
    {
      AddError("expected function name");
      break;
    }
    ExpressionNodePtr identifier_node =
        CreateExpressionNode(NodeKind::Identifier, token_->text, token_->line);
    function_definition_node->children.push_back(std::move(identifier_node));
    Next();
    if (token_->kind != Token::Kind::LeftParenthesis)
    {
      AddError("expected '('");
      break;
    }
    Next();
    if (token_->kind != Token::Kind::RightParenthesis)
    {
      bool inner_ok = false;
      int  count    = 0;
      do
      {
        if (token_->kind != Token::Kind::Identifier)
        {
          if (count)
          {
            AddError("expected parameter name");
          }
          else
          {
            AddError("expected parameter name or ')'");
          }
          break;
        }
        ExpressionNodePtr parameter_node =
            CreateExpressionNode(NodeKind::Identifier, token_->text, token_->line);
        function_definition_node->children.push_back(std::move(parameter_node));
        Next();
        if (token_->kind != Token::Kind::Colon)
        {
          AddError("expected ':'");
          break;
        }
        ExpressionNodePtr type_node = ParseType();
        if (type_node == nullptr)
        {
          break;
        }
        function_definition_node->children.push_back(std::move(type_node));
        Next();
        if (token_->kind == Token::Kind::RightParenthesis)
        {
          inner_ok = true;
          break;
        }
        if (token_->kind != Token::Kind::Comma)
        {
          AddError("expected ',' or ')'");
          break;
        }
        Next();
        ++count;
      } while (true);
      if (!inner_ok)
      {
        break;
      }
    }
    // Scan for optional return type
    ExpressionNodePtr return_type_node;
    Next();
    if (token_->kind == Token::Kind::Colon)
    {
      return_type_node = ParseType();
      if (return_type_node == nullptr)
      {
        break;
      }
    }
    else
    {
      Undo();
    }
    // NOTE: the return type node is legitimately null if no return type ia supplied
    function_definition_node->children.push_back(return_type_node);
    ok = true;
  } while (false);
  if (!ok)
  {
    SkipFunctionDefinition();
    return {};
  }
  if (!ParseBlock(*function_definition_node))
  {
    return {};
  }
  return function_definition_node;
}

NodePtr Parser::ParseAnnotations()
{
  NodePtr annotations_node = CreateBasicNode(NodeKind::Annotations, token_->text, token_->line);
  do
  {
    NodePtr annotation_node = ParseAnnotation();
    if (annotation_node == nullptr)
    {
      return {};
    }
    annotations_node->children.push_back(annotation_node);
  } while (token_->kind == Token::Kind::AnnotationIdentifier);
  return annotations_node;
}

NodePtr Parser::ParseAnnotation()
{
  NodePtr annotation_node = CreateBasicNode(NodeKind::Annotation, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::LeftParenthesis)
  {
    return annotation_node;
  }
  do
  {
    ExpressionNodePtr node = ParseAnnotationLiteral();
    if (node == nullptr)
    {
      return {};
    }
    if ((node->node_kind == NodeKind::String) || (node->node_kind == NodeKind::Identifier))
    {
      Next();
      if (token_->kind != Token::Kind::Assign)
      {
        annotation_node->children.push_back(node);
      }
      else
      {
        NodePtr pair_node =
            CreateBasicNode(NodeKind::AnnotationNameValuePair, token_->text, token_->line);
        pair_node->children.push_back(node);
        ExpressionNodePtr value_node = ParseAnnotationLiteral();
        if (value_node == nullptr)
        {
          return {};
        }
        pair_node->children.push_back(value_node);
        annotation_node->children.push_back(pair_node);
        Next();
      }
    }
    else
    {
      annotation_node->children.push_back(node);
      Next();
    }
    if (token_->kind == Token::Kind::RightParenthesis)
    {
      Next();
      return annotation_node;
    }
    if (token_->kind != Token::Kind::Comma)
    {
      AddError("expected ',' or ')'");
      return {};
    }
  } while (true);
}

ExpressionNodePtr Parser::ParseAnnotationLiteral()
{
  NodeKind kind;
  int      sign   = 0;
  bool     number = false;
  Next();
  if (token_->kind == Token::Kind::Plus)
  {
    sign = 1;
    Next();
  }
  else if (token_->kind == Token::Kind::Minus)
  {
    sign = -1;
    Next();
  }
  switch (token_->kind)
  {
  case Token::Kind::Integer32:
  {
    kind   = NodeKind::Integer64;
    number = true;
    break;
  }
  case Token::Kind::Float64:
  {
    kind   = NodeKind::Float64;
    number = true;
    break;
  }
  case Token::Kind::True:
  {
    kind = NodeKind::True;
    break;
  }
  case Token::Kind::False:
  {
    kind = NodeKind::False;
    break;
  }
  case Token::Kind::String:
  {
    kind = NodeKind::String;
    break;
  }
  case Token::Kind::Identifier:
  {
    kind = NodeKind::Identifier;
    break;
  }
  default:
  {
    AddError("expected annotation literal");
    return {};
  }
  }  // switch
  ExpressionNodePtr node = CreateExpressionNode(kind, token_->text, token_->line);
  if (sign != 0)
  {
    if (number)
    {
      if (sign == -1)
      {
        node->text = "-" + node->text;
      }
    }
    else
    {
      AddError("expected number");
      return {};
    }
  }
  return node;
}

BlockNodePtr Parser::ParseWhileStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("while loop not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  BlockNodePtr while_statement_node =
      CreateBlockNode(NodeKind::WhileStatement, token_->text, token_->line);
  ExpressionNodePtr expression = ParseConditionalExpression();
  if (expression == nullptr)
  {
    return {};
  }
  while_statement_node->children.push_back(std::move(expression));
  if (!ParseBlock(*while_statement_node))
  {
    return {};
  }
  return while_statement_node;
}

BlockNodePtr Parser::ParseForStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("for loop not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  BlockNodePtr for_statement_node =
      CreateBlockNode(NodeKind::ForStatement, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::LeftParenthesis)
  {
    AddError("expected '('");
    return {};
  }
  Next();
  if (token_->kind != Token::Kind::Identifier)
  {
    AddError("expected identifier");
    return {};
  }
  ExpressionNodePtr identifier_node =
      CreateExpressionNode(NodeKind::Identifier, token_->text, token_->line);
  for_statement_node->children.push_back(std::move(identifier_node));
  Next();
  if (token_->kind != Token::Kind::In)
  {
    AddError("expected 'in'");
    return {};
  }
  ExpressionNodePtr part1 = ParseExpression();
  if (part1 == nullptr)
  {
    return {};
  }
  for_statement_node->children.push_back(std::move(part1));
  Next();
  if (token_->kind != Token::Kind::Colon)
  {
    AddError("expected ':'");
    return {};
  }
  ExpressionNodePtr part2 = ParseExpression();
  if (part2 == nullptr)
  {
    return {};
  }
  for_statement_node->children.push_back(std::move(part2));
  Next();
  if (token_->kind == Token::Kind::Colon)
  {
    ExpressionNodePtr part3 = ParseExpression();
    if (part3 == nullptr)
    {
      return {};
    }
    for_statement_node->children.push_back(std::move(part3));
    Next();
  }
  if (token_->kind != Token::Kind::RightParenthesis)
  {
    AddError("expected ')'");
    return {};
  }
  if (!ParseBlock(*for_statement_node))
  {
    return {};
  }
  return for_statement_node;
}

NodePtr Parser::ParseIfStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("if statement not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  NodePtr if_statement_node = CreateBasicNode(NodeKind::IfStatement, token_->text, token_->line);
  do
  {
    if (token_->kind == Token::Kind::If)
    {
      BlockNodePtr      if_node         = CreateBlockNode(NodeKind::If, token_->text, token_->line);
      ExpressionNodePtr expression_node = ParseConditionalExpression();
      if (expression_node == nullptr)
      {
        return {};
      }
      if_node->children.push_back(std::move(expression_node));
      if (!ParseBlock(*if_node))
      {
        return {};
      }
      if_statement_node->children.push_back(std::move(if_node));
      continue;
    }
    else if (token_->kind == Token::Kind::ElseIf)
    {
      BlockNodePtr      elseif_node = CreateBlockNode(NodeKind::ElseIf, token_->text, token_->line);
      ExpressionNodePtr expression_node = ParseConditionalExpression();
      if (expression_node == nullptr)
      {
        return {};
      }
      elseif_node->children.push_back(std::move(expression_node));
      if (!ParseBlock(*elseif_node))
      {
        return {};
      }
      if_statement_node->children.push_back(std::move(elseif_node));
      continue;
    }
    else if (token_->kind == Token::Kind::Else)
    {
      BlockNodePtr else_node = CreateBlockNode(NodeKind::Else, token_->text, token_->line);
      if (!ParseBlock(*else_node))
      {
        return {};
      }
      if_statement_node->children.push_back(std::move(else_node));
      continue;
    }
    return if_statement_node;
  } while (true);
}

NodePtr Parser::ParseVarStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("variable declaration not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  NodePtr var_statement_node =
      CreateBasicNode(NodeKind::VarDeclarationStatement, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::Identifier)
  {
    AddError("expected variable name");
    return {};
  }
  ExpressionNodePtr identifier_node =
      CreateExpressionNode(NodeKind::Identifier, token_->text, token_->line);
  var_statement_node->children.push_back(std::move(identifier_node));
  bool type   = false;
  bool assign = false;
  Next();
  if (token_->kind == Token::Kind::Colon)
  {
    ExpressionNodePtr type_annotation_node = ParseType();
    if (type_annotation_node == nullptr)
    {
      return {};
    }
    var_statement_node->children.push_back(std::move(type_annotation_node));
    type = true;
    Next();
  }
  if (token_->kind == Token::Kind::Assign)
  {
    ExpressionNodePtr expression_node = ParseExpression();
    if (expression_node == nullptr)
    {
      return {};
    }
    var_statement_node->children.push_back(std::move(expression_node));
    assign = true;
    Next();
  }
  if (token_->kind != Token::Kind::SemiColon)
  {
    if (assign)
    {
      AddError("expected ';'");
    }
    else if (type)
    {
      AddError("expected '=' or ';'");
    }
    else
    {
      AddError("expected ':' or '='");
    }
    return {};
  }
  if (!type)
  {
    if (!assign)
    {
      AddError("expected ':' or '='");
      return {};
    }
    var_statement_node->node_kind = NodeKind::VarDeclarationTypelessAssignmentStatement;
  }
  else if (assign)
  {
    var_statement_node->node_kind = NodeKind::VarDeclarationTypedAssignmentStatement;
  }
  return var_statement_node;
}

NodePtr Parser::ParseReturnStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("return statement not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  NodePtr return_statement_node =
      CreateBasicNode(NodeKind::ReturnStatement, token_->text, token_->line);
  Next();
  if (token_->kind == Token::Kind::SemiColon)
  {
    return return_statement_node;
  }
  if (token_->kind == Token::Kind::EndOfInput)
  {
    AddError("expected expression or ';'");
    return {};
  }
  Undo();
  ExpressionNodePtr expression = ParseExpression();
  if (expression == nullptr)
  {
    return {};
  }
  Next();
  if (token_->kind != Token::Kind::SemiColon)
  {
    AddError("expected ';'");
    return {};
  }
  return_statement_node->children.push_back(std::move(expression));
  return return_statement_node;
}

NodePtr Parser::ParseBreakStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("break statement not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  NodePtr break_statement_node =
      CreateBasicNode(NodeKind::BreakStatement, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::SemiColon)
  {
    AddError("expected ';'");
    return {};
  }
  return break_statement_node;
}

NodePtr Parser::ParseContinueStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    AddError("continue statement not permitted at topmost scope");
    // Move one token on so GoToNextStatement() can work properly
    Next();
    return {};
  }
  NodePtr continue_statement_node =
      CreateBasicNode(NodeKind::ContinueStatement, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::SemiColon)
  {
    AddError("expected ';'");
    return {};
  }
  return continue_statement_node;
}

ExpressionNodePtr Parser::ParseExpressionStatement()
{
  NodeKind const block_kind = blocks_.back();
  if (block_kind == NodeKind::File)
  {
    // Get the first token of the expression
    Next();
    if (token_->kind != Token::Kind::Unknown)
    {
      AddError("expression statement not permitted at topmost scope");
    }
    else
    {
      AddError("unrecognised token");
    }
    return {};
  }
  ExpressionNodePtr lhs = ParseExpression();
  if (lhs == nullptr)
  {
    return {};
  }
  Next();
  NodeKind kind;
  switch (token_->kind)
  {
	  case Token::Kind::SemiColon:
		  // Non-assignment expression statement
		  return lhs;
	  case Token::Kind::Assign:
		  kind = NodeKind::Assign;;
		  break;
	  case Token::Kind::InplaceAdd:
		  kind = NodeKind::InplaceAdd;;
		  break;
	  case Token::Kind::InplaceSubtract:
		  kind = NodeKind::InplaceSubtract;;
		  break;
	  case Token::Kind::InplaceMultiply:
		  kind = NodeKind::InplaceMultiply;;
		  break;
	  case Token::Kind::InplaceDivide:
		  kind = NodeKind::InplaceDivide;;
		  break;
	  case Token::Kind::InplaceModulo:
		  kind = NodeKind::InplaceModulo;;
		  break;
	  default:
		  AddError("expected ';' or assignment operator");
		  return {};
  }
  // NOTE: This doesn't handle chains of assignment a = b = c by design
  ExpressionNodePtr assignment_node = CreateExpressionNode(kind, token_->text, token_->line);
  ExpressionNodePtr rhs             = ParseExpression();
  if (rhs == nullptr)
  {
    return {};
  }
  assignment_node->children.push_back(std::move(lhs));
  assignment_node->children.push_back(std::move(rhs));
  Next();
  if (token_->kind == Token::Kind::SemiColon)
  {
    return assignment_node;
  }
  AddError("expected ';'");
  return {};
}

void Parser::GoToNextStatement()
{
  while (!value_util::IsAnyOf(token_->kind, Token::Kind::EndOfInput, Token::Kind::SemiColon))
  {
    if (value_util::IsAnyOf(token_->kind, Token::Kind::AnnotationIdentifier, Token::Kind::Function, Token::Kind::While, Token::Kind::For, Token::Kind::If, Token::Kind::Var, Token::Kind::Return, Token::Kind::Break, Token::Kind::Continue))
    {
      Undo();
      return;
    }
    Next();
  }
}

void Parser::SkipFunctionDefinition()
{
  while (!value_util::IsAnyOf(token_->kind, Token::Kind::EndOfInput, Token::Kind::EndFunction))
  {
    Next();
  }
}

bool Parser::IsTemplateName(std::string const &name) const
{
  auto const it = template_names_.find(name);
  return template_names_.end() != it;
}

ExpressionNodePtr Parser::ParseType()
{
  Next();
  if (token_->kind != Token::Kind::Identifier)
  {
    AddError("expected type name");
    return {};
  }
  std::string       name = token_->text;
  ExpressionNodePtr identifier_node =
      CreateExpressionNode(NodeKind::Identifier, token_->text, token_->line);
  if (!IsTemplateName(name))
  {
    return identifier_node;
  }
  Next();
  if (token_->kind != Token::Kind::LessThan)
  {
    AddError("expected '<'");
    return {};
  }
  name += "<";
  ExpressionNodePtr template_node =
      CreateExpressionNode(NodeKind::Template, token_->text, token_->line);
  template_node->children.push_back(std::move(identifier_node));
  do
  {
    ExpressionNodePtr subtype_node = ParseType();
    if (subtype_node == nullptr)
    {
      return {};
    }
    name += subtype_node->text;
    template_node->children.push_back(std::move(subtype_node));
    Next();
    if (token_->kind == Token::Kind::Comma)
    {
      name += ",";
      continue;
    }
    if (token_->kind == Token::Kind::GreaterThan)
    {
      name += ">";
      template_node->text = name;
      return template_node;
    }
    AddError("expected ',' or '>'");
    return {};
  } while (true);
}

ExpressionNodePtr Parser::ParseConditionalExpression()
{
  Next();
  if (token_->kind != Token::Kind::LeftParenthesis)
  {
    AddError("expected '('");
    return {};
  }
  Undo();
  return ParseExpression(true);
}

ExpressionNodePtr Parser::ParseExpression(bool is_conditional_expression)
{
  state_                       = State::PreOperand;
  found_expression_terminator_ = false;
  groups_.clear();
  operators_.clear();
  rpn_.clear();
  infix_stack_.clear();

  static const std::unordered_map<Token::Kind, NodeKind> literal_types{
	  {Token::Kind::Integer8, NodeKind::Integer8},
	  {Token::Kind::Integer16, NodeKind::Integer16},
	  {Token::Kind::Integer32, NodeKind::Integer32},
	  {Token::Kind::Integer64, NodeKind::Integer64},
	  {Token::Kind::UnsignedInteger8, NodeKind::UnsignedInteger8},
	  {Token::Kind::UnsignedInteger16, NodeKind::UnsignedInteger16},
	  {Token::Kind::UnsignedInteger32, NodeKind::UnsignedInteger32},
	  {Token::Kind::UnsignedInteger64, NodeKind::UnsignedInteger64},
	  {Token::Kind::Float32, NodeKind::Float32},
	  {Token::Kind::Float64, NodeKind::Float64},
	  {Token::Kind::String, NodeKind::String},
	  {Token::Kind::True, NodeKind::True},
	  {Token::Kind::False, NodeKind::False},
	  {Token::Kind::Null, NodeKind::Null},
  };

  struct NodeOpInfo {
	  NodeKind kind;
	  OpInfo op_info;
  };

  static const std::unordered_map<Token::Kind, NodeOpInfo> binary_operators{
	  {Token::Kind::Multiply, NodeOpInfo{NodeKind::Multiply, OpInfo{6, Association::Left, 2}}},
		  {Token::Kind::Divide, NodeOpInfo{NodeKind::Divide, OpInfo{6, Association::Left, 2}}},
		  {Token::Kind::Modulo, NodeOpInfo{NodeKind::Modulo, OpInfo{6, Association::Left, 2}}},
		  {Token::Kind::Equal, NodeOpInfo{NodeKind::Equal, OpInfo{3, Association::Left, 2}}},
		  {Token::Kind::NotEqual, NodeOpInfo{NodeKind::NotEqual, OpInfo{3, Association::Left, 2}}},
		  {Token::Kind::LessThan, NodeOpInfo{NodeKind::LessThan, OpInfo{4, Association::Left, 2}}},
		  {Token::Kind::LessThanOrEqual, NodeOpInfo{NodeKind::LessThanOrEqual, OpInfo{4, Association::Left, 2}}},
		  {Token::Kind::GreaterThan, NodeOpInfo{NodeKind::GreaterThan, OpInfo{4, Association::Left, 2}}},
		  {Token::Kind::GreaterThanOrEqual, NodeOpInfo{NodeKind::GreaterThanOrEqual, OpInfo{4, Association::Left, 2}}},
		  {Token::Kind::And, NodeOpInfo{NodeKind::And, OpInfo{2, Association::Left, 2}}},
		  {Token::Kind::Or, NodeOpInfo{NodeKind::Or, OpInfo{1, Association::Left, 2}}}
  };

  do
  {
    bool parses{};
    Next();
    switch (token_->kind)
    {
    case Token::Kind::Identifier:
      parses = HandleIdentifier();
      break;

    case Token::Kind::Integer8:
    case Token::Kind::Integer16:
    case Token::Kind::Integer32:
    case Token::Kind::Integer64:
    case Token::Kind::UnsignedInteger8:
    case Token::Kind::UnsignedInteger16:
    case Token::Kind::UnsignedInteger32:
    case Token::Kind::UnsignedInteger64:
    case Token::Kind::Float32:
    case Token::Kind::Float64:
    case Token::Kind::String:
    case Token::Kind::True:
    case Token::Kind::False:
    case Token::Kind::Null:
      parses = HandleLiteral(literal_types[token_->kind]);
      break;

    case Token::Kind::Plus:
      HandlePlus();
      parses = true;
      break;

    case Token::Kind::Minus:
      HandleMinus();
      parses = true;
      break;

    case Token::Kind::Multiply:
    case Token::Kind::Divide:
    case Token::Kind::Modulo:
    case Token::Kind::Equal:
    case Token::Kind::NotEqual:
    case Token::Kind::LessThan:
    case Token::Kind::LessThanOrEqual:
    case Token::Kind::GreaterThan:
    case Token::Kind::GreaterThanOrEqual:
    case Token::Kind::And:
    case Token::Kind::Or:
    {
      auto const &binary_operator = binary_operators[token_->kind];
      parses = HandleBinaryOp(binary_operator.kind, binary_operator.op_info);
      break;
    }

    case Token::Kind::Not:
      HandleNot();
      break;

    case Token::Kind::Inc:
      HandlePrefixPostfix(NodeKind::PrefixInc, OpInfo(7, Association::Right, 1),
                          NodeKind::PostfixInc, OpInfo(8, Association::Left, 1));
      break;

    case Token::Kind::Dec:
      HandlePrefixPostfix(NodeKind::PrefixDec, OpInfo(7, Association::Right, 1),
                          NodeKind::PostfixDec, OpInfo(8, Association::Left, 1));
      break;

    case Token::Kind::LeftParenthesis:
      parses = HandleOpener(NodeKind::ParenthesisGroup, NodeKind::Invoke, Token::Kind::RightParenthesis, ")");
      break;

    case Token::Kind::LeftSquareBracket:
      parses = HandleLeftSquareBracket();
      break;

    case Token::Kind::RightParenthesis:
    case Token::Kind::RightSquareBracket:
      parses = HandleCloser(is_conditional_expression);
      break;

    case Token::Kind::Dot:
      parses = HandleDot();
      break;

    case Token::Kind::Comma:
      parses = HandleComma();
      break;

    default:
    {
      if (state_ == State::PreOperand)
      {
        if (token_->kind != Token::Kind::Unknown)
        {
          AddError("expected expression");
        }
        else
        {
          AddError("unrecognised token");
        }
        return {};
      }
      found_expression_terminator_ = true;
      break;
    }
    }  // switch
    if (!parses)
    {
      return {}; // failed parse
    }
  } while (!found_expression_terminator_);
  if (!groups_.empty())
  {
    Expr const &groupop = operators_[groups_.back()];
    AddError("expected '" + groupop.closer_token_text + "'");
    return {};
  }
  // Rollback so token_ is pointing at the last token of the expression
  Undo();
  rpn_.reserve(rpn_.size() + operators_.size());
  std::move(operators_.rbegin(), operators_.rend(), std::back_inserter(rpn_));
  operators_.clear();
  // rpn_ holds the Reverse Polish Notation (aka postfix) expression
  // Here we convert the RPN to an infix expression tree
  for (auto &expr: rpn_)
  {
    if (value_util::IsAnyOf(expr.node->node_kind, NodeKind::ParenthesisGroup, NodeKind::UnaryPlus))
    {
      // Just ignore these no-ops
      continue;
    }
    if (expr.is_operator)
    {
      std::size_t const arity = std::size_t(expr.op_info.arity);
      if (arity)
      {
        auto expr_position = infix_stack_.end() - arity;
	// move arity last nodes into expr's arguments
        std::transform(expr_position, infix_stack_.end(), std::back_inserter(expr.node->children),
		       [](Expr &operand)->Node { return std::move(operand.node); })
	// hide infix_stack_'s tail in expr subtree
	*expr_position = std::move(expr);
	infix_stack_.erase(expr_position + 1, infix_stack_.end());
	continue;
      }
    }
    // expr is not an operator, or its arity is 0
    infix_stack_.push_back(std::move(expr));
  }
  // Return the root of the infix expression tree
  return std::move(infix_stack_.front().node);
}

bool Parser::HandleIdentifier()
{
  if (state_ == State::PostOperand)
  {
    found_expression_terminator_ = true;
    return true;
  }
  std::string name;
  if (!ParseExpressionIdentifier(name))
  {
    return false;
  }
  state_ = State::PostOperand;
  return true;
}

bool Parser::ParseExpressionIdentifier(std::string &name)
{
  AddOperand(NodeKind::Identifier);
  name = token_->text;
  if (!IsTemplateName(name))
  {
    return true;
  }
  Next();
  if (token_->kind != Token::Kind::LessThan)
  {
    AddError("expected '<'");
    return false;
  }
  name += "<";
  AddGroup(NodeKind::Template, 1, Token::Kind::GreaterThan, ">");
  do
  {
    Next();
    if (token_->kind != Token::Kind::Identifier)
    {
      AddError("expected type name");
      return false;
    }
    std::string subtypename;
    if (!ParseExpressionIdentifier(subtypename))
    {
      return false;
    }
    name += subtypename;
    Next();
    if (token_->kind == Token::Kind::Comma)
    {
      name += ",";
      Expr &groupop = operators_.back();
      (groupop.op_info.arity)++;
      continue;
    }
    if (token_->kind == Token::Kind::GreaterThan)
    {
      name += ">";
      Expr &groupop      = operators_.back();
      groupop.node->text = name;
      (groupop.op_info.arity)++;
      groups_.pop_back();
      rpn_.push_back(std::move(groupop));
      operators_.pop_back();
      return true;
    }
    AddError("expected ',' or '>'");
    return false;
  } while (true);
}

bool Parser::HandleLiteral(NodeKind kind)
{
  if (state_ == State::PostOperand)
  {
    found_expression_terminator_ = true;
    return true;
  }
  AddOperand(kind);
  state_ = State::PostOperand;
  return true;
}

void Parser::HandlePlus()
{
  if (state_ == State::PreOperand)
  {
    HandleOp(NodeKind::UnaryPlus, OpInfo(7, Association::Right, 1));
  }
  else
  {
    HandleOp(NodeKind::Add, OpInfo(5, Association::Left, 2));
    state_ = State::PreOperand;
  }
}

void Parser::HandleMinus()
{
  if (state_ == State::PreOperand)
  {
    HandleOp(NodeKind::Negate, OpInfo(7, Association::Right, 1));
  }
  else
  {
    HandleOp(NodeKind::Subtract, OpInfo(5, Association::Left, 2));
    state_ = State::PreOperand;
  }
}

bool Parser::HandleBinaryOp(NodeKind kind, OpInfo const &op_info)
{
  if (state_ == State::PreOperand)
  {
    AddError("expected expression");
    return false;
  }
  HandleOp(kind, op_info);
  state_ = State::PreOperand;
  return true;
}

void Parser::HandleNot()
{
  if (state_ == State::PreOperand)
  {
    HandleOp(NodeKind::Not, OpInfo(7, Association::Right, 1));
  }
  else
  {
    found_expression_terminator_ = true;
  }
}

void Parser::HandlePrefixPostfix(NodeKind prefix_kind, OpInfo const &prefix_op_info,
                                 NodeKind postfix_kind, OpInfo const &postfix_op_info)
{
  if (state_ == State::PreOperand)
  {
    HandleOp(prefix_kind, prefix_op_info);
  }
  else
  {
    HandleOp(postfix_kind, postfix_op_info);
  }
}

bool Parser::HandleDot()
{
  if (state_ == State::PreOperand)
  {
    AddError("expected expression");
    return false;
  }
  ExpressionNodePtr dot_node = CreateExpressionNode(NodeKind::Dot, token_->text, token_->line);
  Next();
  if (token_->kind != Token::Kind::Identifier)
  {
    AddError("expected identifier");
    return false;
  }
  AddOperand(NodeKind::Identifier);
  IncrementGroupMembers();
  Expr expr;
  expr.is_operator       = true;
  expr.node              = std::move(dot_node);
  expr.op_info.arity     = 2;
  rpn_.push_back(std::move(expr));
  state_ = State::PostOperand;
  return true;
}

bool Parser::HandleLeftSquareBracket()
{
  if (state_ == State::PreOperand)
  {
    return HandleArrayExpression();
  }
  return HandleOpener(NodeKind::Unknown, NodeKind::Index, Token::Kind::RightSquareBracket, "]");
}

bool Parser::HandleArrayExpression()
{
  auto value{ParseArray()};
  if (!value)
  {
    return false;
  }
  IncrementGroupMembers();
  Expr expr;
  expr.is_operator = false;
  expr.node = std::move(value);
  rpn_.push_back(std::move(expr));
  return true;
}

// ArrayExpression ::= "[" Array
// Left square bracket has been already consumed.
//
// Array ::= ArraySequence | ArrayRepetition | ArrayEmpty
ExpressionNodePtr ParseArray()
{
	return value_util::Or(ParseArraySequence(), ParseArrayRepetition(), ParseArrayEmpty());
}

// ArraySequence ::= Expr ("," Expr)* "]"
ExpressionNodePtr ParseArraySequence()
{
	NodePtrArray elements;
	for(auto element{ParseExpression()}; element; element = value_util::And(MatchLiteral(Token::Kind::Comma), ParseExpression()))
	{
		elements.push_back(std::move(element));
	}
	if (elements.empty())
	{
		return {};
	}
	Next();
	if (token_->kind == Token::Kind::RightSquareBracket)
	{
		// empty array literal
		return ExpressionNodePtr{ExpessionKind::RV, TemplateInstance(
	}
	switch (token_->kind)
	{
		case Token::Kind::RightSquareBracket: return EmptyArray();
			c

bool Parser::HandleOpener(NodeKind prefix_kind, NodeKind postfix_kind,
                          Token::Kind closer_token_kind, std::string const &closer_token_text)
{
  if (state_ == State::PreOperand)
  {
    if (prefix_kind == NodeKind::Unknown)
    {
      AddError("expected expression");
      return false;
    }
    AddGroup(prefix_kind, 0, closer_token_kind, closer_token_text);
    return true;
  }
  if (postfix_kind == NodeKind::Unknown)
  {
    found_expression_terminator_ = true;
    return true;
  }
  AddGroup(postfix_kind, 1, closer_token_kind, closer_token_text);
  state_ = State::PreOperand;
  return true;
}

bool Parser::HandleCloser(bool is_conditional_expression)
{
  if (groups_.empty())
  {
    // Closer without an opener
    if (state_ == State::PreOperand)
    {
      AddError("expected expression");
      return false;
    }
    found_expression_terminator_ = true;
    return true;
  }
  Expr const groupop = operators_[groups_.back()];
  if (token_->kind != groupop.closer_token_kind)
  {
    // Opener and closer don't match
    // Regardless of state_, this is a syntax error
    AddError("expected '" + groupop.closer_token_text + "'");
    return false;
  }
  while (operators_.size())
  {
    Expr &topop = operators_.back();
    if (topop.node->node_kind != groupop.node->node_kind)
    {
      rpn_.push_back(std::move(topop));
      operators_.pop_back();
      continue;
    }
    if (groupop.num_members > 0)
    {
      // Non-empty group
      if (state_ == State::PreOperand)
      {
        AddError("expected expression");
        return false;
      }
      (topop.op_info.arity)++;
    }
    else
    {
      // Empty group
      if ((groupop.node->node_kind == NodeKind::ParenthesisGroup) ||
          (groupop.node->node_kind == NodeKind::Index))
      {
        AddError("expected expression");
        return false;
      }
    }
    if ((groupop.node->node_kind == NodeKind::ParenthesisGroup) && (groups_.size() == 1) &&
        (is_conditional_expression))
    {
      // We've found the final closing bracket of a conditional expression
      Next();
      found_expression_terminator_ = true;
    }
    groups_.pop_back();
    rpn_.push_back(std::move(topop));
    operators_.pop_back();
    break;
  }
  state_ = State::PostOperand;
  return true;
}

bool Parser::HandleComma()
{
  if (state_ == State::PreOperand)
  {
    AddError("expected expression");
    return false;
  }
  if (groups_.empty())
  {
    // Commas must be inside a group
    found_expression_terminator_ = true;
    return true;
  }
  Expr const groupop = operators_[groups_.back()];
  if (groupop.node->node_kind == NodeKind::ParenthesisGroup)
  {
    // Commas are not allowed inside a parenthesis group
    AddError("");
    return false;
  }
  while (operators_.size())
  {
    Expr &topop = operators_.back();
    if (topop.node->node_kind == groupop.node->node_kind)
    {
      (topop.op_info.arity)++;
      break;
    }
    rpn_.push_back(std::move(topop));
    operators_.pop_back();
  }
  state_ = State::PreOperand;
  return true;
}

void Parser::HandleOp(NodeKind kind, OpInfo const &op_info)
{
  NodeKind group_kind;
  bool     check_if_group_opener = false;
  if (groups_.size())
  {
    Expr const &groupop   = operators_[groups_.back()];
    group_kind            = groupop.node->node_kind;
    check_if_group_opener = true;
  }
  while (operators_.size())
  {
    Expr &topop = operators_.back();
    if ((check_if_group_opener) && (topop.node->node_kind == group_kind))
    {
      break;
    }
    if (op_info.precedence > topop.op_info.precedence)
    {
      break;
    }
    if (op_info.precedence == topop.op_info.precedence)
    {
      if (op_info.association == Association::Right)
      {
        break;
      }
    }
    rpn_.push_back(std::move(topop));
    operators_.pop_back();
  }
  AddOp(kind, op_info);
}

void Parser::AddGroup(NodeKind kind, int arity, Token::Kind closer_token_kind,
                      std::string const &closer_token_text)
{
  IncrementGroupMembers();
  Expr expr;
  expr.is_operator       = true;
  expr.node              = CreateExpressionNode(kind, token_->text, token_->line);
  expr.op_info.arity     = arity;
  expr.closer_token_kind = closer_token_kind;
  expr.closer_token_text = closer_token_text;
  groups_.push_back(operators_.size());
  operators_.push_back(std::move(expr));
}

void Parser::AddOp(NodeKind kind, OpInfo const &op_info)
{
  IncrementGroupMembers();
  Expr expr;
  expr.is_operator       = true;
  expr.node              = CreateExpressionNode(kind, token_->text, token_->line);
  expr.op_info           = op_info;
  operators_.push_back(std::move(expr));
}

void Parser::AddOperand(NodeKind kind)
{
  IncrementGroupMembers();
  Expr expr;
  expr.is_operator       = false;
  expr.node              = CreateExpressionNode(kind, token_->text, token_->line);
  rpn_.push_back(std::move(expr));
}

void Parser::AddError(std::string const &message)
{
  std::stringstream stream;
  stream << "line " << token_->line << ": ";
  if (token_->kind != Token::Kind::EndOfInput)
  {
    stream << "error at '" << token_->text << "'";
  }
  else
  {
    stream << "reached end-of-input";
  }
  if (message.length())
  {
    stream << ", " << message;
  }
  errors_.push_back(stream.str());
}

}  // namespace vm
}  // namespace fetch
