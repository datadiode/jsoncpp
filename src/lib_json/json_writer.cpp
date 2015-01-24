// Copyright 2011 Baptiste Lepilleur
// Distributed under MIT license, or public domain if desired and
// recognized in your jurisdiction.
// See file LICENSE for detail or copy at http://jsoncpp.sourceforge.net/LICENSE

#if !defined(JSON_IS_AMALGAMATION)
#include <json/writer.h>
#include "json_tool.h"
#endif // if !defined(JSON_IS_AMALGAMATION)
#include <utility>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sstream>
#include <iomanip>
#include <math.h>

#if defined(_MSC_VER)
#include <float.h>
#define isfinite _finite
#define snprintf _snprintf
#endif

#if defined(_MSC_VER) && _MSC_VER >= 1400 // VC++ 8.0
// Disable warning about strdup being deprecated.
#pragma warning(disable : 4996)
#endif

#if defined(__sun) && defined(__SVR4) //Solaris
#include <ieeefp.h>
#define isfinite finite
#endif

namespace Json {

static bool containsControlCharacter(const char* str) {
  while (*str) {
    if (isControlCharacter(*(str++)))
      return true;
  }
  return false;
}

std::string valueToString(LargestInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  bool isNegative = value < 0;
  if (isNegative)
    value = -value;
  uintToString(LargestUInt(value), current);
  if (isNegative)
    *--current = '-';
  assert(current >= buffer);
  return current;
}

std::string valueToString(LargestUInt value) {
  UIntToStringBuffer buffer;
  char* current = buffer + sizeof(buffer);
  uintToString(value, current);
  assert(current >= buffer);
  return current;
}

#if defined(JSON_HAS_INT64)

std::string valueToString(Int value) {
  return valueToString(LargestInt(value));
}

std::string valueToString(UInt value) {
  return valueToString(LargestUInt(value));
}

#endif // # if defined(JSON_HAS_INT64)

std::string valueToString(double value) {
  // Don't let infinite/NaN literals go into output stream.
  if (!isfinite(value)) {
    // IEEE standard states that NaN values will not compare to themselves
    return value != value ? "null" : &"-1e+9999"[value >= 0];
  }
  // Allocate a buffer that is more than large enough to store the 16 digits of
  // precision requested below.
  char buffer[32];
  // Print into the buffer. We need not request the alternative representation
  // that always has a decimal point because JSON doesn't distingish the
  // concepts of reals and integers.
#if defined(_MSC_VER) && defined(__STDC_SECURE_LIB__) // Use secure version with
                                                      // visual studio 2005 to
                                                      // avoid warning.
#if defined(WINCE)
  int len = _snprintf(buffer, sizeof(buffer), "%.17g", value);
#else
  int len = sprintf_s(buffer, sizeof(buffer), "%.17g", value);
#endif
#else
  int len = snprintf(buffer, sizeof(buffer), "%.17g", value);
#endif
  assert(len >= 0);
  fixNumericLocale(buffer, buffer + len);
  return buffer;
}

std::string valueToString(bool value) { return value ? "true" : "false"; }

std::string valueToQuotedString(const char* value) {
  if (value == NULL)
    return "";
  // Not sure how to handle unicode...
  if (strpbrk(value, "\"\\\b\f\n\r\t") == NULL &&
      !containsControlCharacter(value))
    return std::string("\"") + value + "\"";
  // We have to walk value and escape any special characters.
  // Appending to std::string is not efficient, but this should be rare.
  // (Note: forward slashes are *not* rare, but I am not escaping them.)
  std::string::size_type maxsize =
      strlen(value) * 2 + 3; // allescaped+quotes+NULL
  std::string result;
  result.reserve(maxsize); // to avoid lots of mallocs
  result += "\"";
  for (const char* c = value; *c != 0; ++c) {
    switch (*c) {
    case '\"':
      result += "\\\"";
      break;
    case '\\':
      result += "\\\\";
      break;
    case '\b':
      result += "\\b";
      break;
    case '\f':
      result += "\\f";
      break;
    case '\n':
      result += "\\n";
      break;
    case '\r':
      result += "\\r";
      break;
    case '\t':
      result += "\\t";
      break;
    // case '/':
    // Even though \/ is considered a legal escape in JSON, a bare
    // slash is also legal, so I see no reason to escape it.
    // (I hope I am not misunderstanding something.
    // blep notes: actually escaping \/ may be useful in javascript to avoid </
    // sequence.
    // Should add a flag to allow this compatibility mode and prevent this
    // sequence from occurring.
    default:
      if (isControlCharacter(*c)) {
        std::ostringstream oss;
        oss << "\\u" << std::hex << std::uppercase << std::setfill('0')
            << std::setw(4) << static_cast<int>(*c);
        result += oss.str();
      } else {
        result += *c;
      }
      break;
    }
  }
  result += "\"";
  return result;
}

// Class Writer
// //////////////////////////////////////////////////////////////////
Writer::~Writer() {}

// Class FastWriter
// //////////////////////////////////////////////////////////////////

FastWriter::FastWriter()
    : yamlCompatiblityEnabled_(false), dropNullPlaceholders_(false),
      omitEndingLineFeed_(false) {}

void FastWriter::enableYAMLCompatibility() { yamlCompatiblityEnabled_ = true; }

void FastWriter::dropNullPlaceholders() { dropNullPlaceholders_ = true; }

void FastWriter::omitEndingLineFeed() { omitEndingLineFeed_ = true; }

std::string FastWriter::write(const Value& root) {
  document_ = "";
  writeValue(root);
  if (!omitEndingLineFeed_)
    document_ += "\n";
  return document_;
}

void FastWriter::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    if (!dropNullPlaceholders_)
      document_ += "null";
    break;
  case intValue:
    document_ += valueToString(value.asLargestInt());
    break;
  case uintValue:
    document_ += valueToString(value.asLargestUInt());
    break;
  case realValue:
    document_ += valueToString(value.asDouble());
    break;
  case stringValue:
    document_ += valueToQuotedString(value.asCString());
    break;
  case booleanValue:
    document_ += valueToString(value.asBool());
    break;
  case arrayValue: {
    document_ += '[';
    int size = value.size();
    for (int index = 0; index < size; ++index) {
      if (index > 0)
        document_ += ',';
      writeValue(value[index]);
    }
    document_ += ']';
  } break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    document_ += '{';
    for (Value::Members::iterator it = members.begin(); it != members.end();
         ++it) {
      const std::string& name = *it;
      if (it != members.begin())
        document_ += ',';
      document_ += valueToQuotedString(name.c_str());
      document_ += yamlCompatiblityEnabled_ ? ": " : ":";
      writeValue(value[name]);
    }
    document_ += '}';
  } break;
  }
}

// Class StyledWriterMethods
// //////////////////////////////////////////////////////////////////

StyledWriterMethods::StyledWriterMethods(std::string::size_type indentSize)
    : rightMargin_(74), indentation_(indentSize, ' '), addChildValues_() {}

void StyledWriterMethods::writeIndent() {
  if (indentString_.empty())
    indentString_ = "\n";
  else
    write(indentString_.c_str());
}

void StyledWriterMethods::indent() { indentString_ += indentation_; }

void StyledWriterMethods::unindent() {
  assert(indentString_.size() >= indentation_.size());
  indentString_.resize(indentString_.size() - indentation_.size());
}

// Class StyledWriter
// //////////////////////////////////////////////////////////////////
StyledWriter::StyledWriter() : StyledWriterMethods(3) {}

std::string StyledWriter::write(const Value& root) {
  document_.resize(0);
  addChildValues_ = false;
  indentString_.resize(0);
  writeCommentBeforeValue(root);
  writeIndent();
  writeValue(root);
  writeCommentAfterValue(root);
  writeIndent();
  return document_;
}

void StyledWriterMethods::writeValue(const Value& value) {
  switch (value.type()) {
  case nullValue:
    pushValue("null");
    break;
  case intValue:
    pushValue(valueToString(value.asLargestInt()));
    break;
  case uintValue:
    pushValue(valueToString(value.asLargestUInt()));
    break;
  case realValue:
    pushValue(valueToString(value.asDouble()));
    break;
  case stringValue:
    pushValue(valueToQuotedString(value.asCString()));
    break;
  case booleanValue:
    pushValue(valueToString(value.asBool()));
    break;
  case arrayValue:
    writeArrayValue(value);
    break;
  case objectValue: {
    Value::Members members(value.getMemberNames());
    if (members.empty())
      pushValue("{}");
    else {
      write("{");
      indent();
      Value::Members::iterator it = members.begin();
      for (;;) {
        const std::string& name = *it;
        const Value& childValue = value[name];
        writeCommentBeforeValue(childValue);
        writeIndent();
        write(valueToQuotedString(name.c_str()).c_str());
        write(" : ");
        // TODO: writeIndent() here for multiline arrays & objects?
        writeValue(childValue);
        if (++it == members.end()) {
          writeCommentAfterValue(childValue);
          break;
        }
        write(",");
        writeCommentAfterValue(childValue);
      }
      unindent();
      writeIndent();
      write("}");
    }
  } break;
  }
}

void StyledWriterMethods::writeArrayValue(const Value& value) {
  unsigned size = value.size();
  if (size == 0)
    pushValue("[]");
  else {
    bool isArrayMultiLine = isMultineArray(value);
    if (isArrayMultiLine) {
      write("[");
      indent();
      bool hasChildValue = !childValues_.empty();
      unsigned index = 0;
      for (;;) {
        const Value& childValue = value[index];
        writeCommentBeforeValue(childValue);
        writeIndent();
        if (hasChildValue)
          write(childValues_[index].c_str());
        else {
          writeValue(childValue);
        }
        if (++index == size) {
          writeCommentAfterValue(childValue);
          break;
        }
        write(",");
        writeCommentAfterValue(childValue);
      }
      unindent();
      writeIndent();
      write("]");
    } else // output on a single line
    {
      assert(childValues_.size() == size);
      write("[ ");
      for (unsigned index = 0; index < size; ++index) {
        if (index > 0)
          write(", ");
        write(childValues_[index].c_str());
      }
      write(" ]");
    }
  }
}

bool StyledWriterMethods::isMultineArray(const Value& value) {
  int size = value.size();
  bool isMultiLine = size * 3 >= rightMargin_;
  childValues_.clear();
  for (int index = 0; index < size && !isMultiLine; ++index) {
    const Value& childValue = value[index];
    isMultiLine =
        isMultiLine || ((childValue.isArray() || childValue.isObject()) &&
                        childValue.size() > 0)
                    || hasCommentForValue(childValue);
  }
  if (!isMultiLine) // check if line length > max line length
  {
    childValues_.reserve(size);
    addChildValues_ = true;
    int lineLength = 4 + (size - 1) * 2; // '[ ' + ', '*n + ' ]'
    for (int index = 0; index < size; ++index) {
      writeValue(value[index]);
      lineLength += int(childValues_[index].length());
    }
    addChildValues_ = false;
    isMultiLine = isMultiLine || lineLength >= rightMargin_;
  }
  return isMultiLine;
}

void StyledWriterMethods::pushValue(const std::string& value) {
  if (addChildValues_)
    childValues_.push_back(value);
  else
    write(value.c_str());
}

void StyledWriterMethods::writeCommentBeforeValue(const Value& value) {
  writeComment(value.getComment(commentBefore));
}

void StyledWriterMethods::writeCommentAfterValue(const Value& value) {
  if (value.hasComment(commentAfterOnSameLine)) {
    write(" ");
    write(value.getComment(commentAfterOnSameLine).c_str());
  }
  writeComment(value.getComment(commentAfter));
}

void StyledWriterMethods::writeComment(std::string text) {
  const char* q = text.c_str();
  while (const char *p = strchr(q, '/')) {
    q = p;
    bool block = false;
    switch (char c = *++q) {
    case '*':
      block = true;
      do {
      case '/':
        do {
          c = *++q;
        } while (c != '\0' && c != '\r' && c != '\n');
      } while (block && c != '\0' && (q[-1] != '/' || q[-2] != '*'));
    }
    writeIndent();
    write(std::string(p, q).c_str());
  }
}

bool StyledWriterMethods::hasCommentForValue(const Value& value) {
  return value.hasComment(commentBefore) ||
         value.hasComment(commentAfterOnSameLine) ||
         value.hasComment(commentAfter);
}


// Class StyledStreamWriter
// //////////////////////////////////////////////////////////////////

StyledStreamWriter::StyledStreamWriter(std::string indentation)
    : document_(NULL) { indentation_.swap(indentation); }

void StyledStreamWriter::write(std::ostream& out, const Value& root) {
  document_ = &out;
  addChildValues_ = false;
  indentString_.resize(0);
  writeCommentBeforeValue(root);
  writeIndent();
  writeValue(root);
  writeCommentAfterValue(root);
  writeIndent();
  document_ = NULL; // Forget the stream, for safety.
}

std::ostream& operator<<(std::ostream& sout, const Value& root) {
  Json::StyledStreamWriter writer;
  writer.write(sout, root);
  return sout;
}

} // namespace Json
