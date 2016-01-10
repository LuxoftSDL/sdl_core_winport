/*
 * Copyright (c) 2016, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SRC_COMPONENTS_UTILS_INCLUDE_UTILS_JSON_UTILS_H_
#define SRC_COMPONENTS_UTILS_INCLUDE_UTILS_JSON_UTILS_H_

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#if defined(QT_PORT)
#include <QVariant>
#else
#include "json/json.h"
#endif

namespace utils {
namespace json {

namespace ValueType {
enum Type {
  NULL_VALUE = 0,  ///< 'null' value
  INT_VALUE,       ///< signed integer value
  UINT_VALUE,      ///< unsigned integer value
  REAL_VALUE,      ///< double value
  STRING_VALUE,    ///< UTF-8 string value
  BOOL_VALUE,      ///< bool value
  ARRAY_VALUE,     ///< array value (ordered list)
  OBJECT_VALUE     ///< object value (collection of name/value pairs).
};
}  // namespace ValueType

class JsonValue {
 public:
  typedef int32_t Int;
  typedef uint32_t UInt;
  typedef uint32_t ArrayIndex;
  typedef std::vector<std::string> Members;
  typedef std::pair<JsonValue, bool> ParseResult;

#if defined(QT_PORT)
  typedef QVariant Storage;
#else
  typedef Json::Value Storage;
#endif

  JsonValue(const char* value);

  JsonValue(const std::string& value);

  JsonValue(Int value);

  JsonValue(UInt value);

  JsonValue(bool value);

  JsonValue(double value);

  JsonValue(ValueType::Type type = ValueType::NULL_VALUE);

  JsonValue(const Storage& value);

  static ParseResult Parse(const std::string& document);

  std::string ToJson() const;

  bool HasMember(const char* key) const;

  bool HasMember(const std::string& key) const;

  JsonValue Get(const char* key, const JsonValue& default_value) const;

  JsonValue Get(const std::string& key, const JsonValue& default_value) const;

  std::string AsString() const;

  Int AsInt() const;

  UInt AsUInt() const;

  bool AsBool() const;

  double AsDouble() const;

  bool IsBool() const;

  bool IsDouble() const;

  bool IsInt() const;

  bool IsUInt() const;

  bool IsString() const;

  bool IsObject() const;

  bool IsNull() const;

  bool IsEmpty() const;

  bool IsArray() const;

  ArrayIndex Size() const;

  ValueType::Type Type() const;

  Members GetMemberNames() const;

  class const_iterator;
  class iterator;

  class Ref {
   public:
    Ref& operator=(const JsonValue& rhs);

    Ref& operator=(const Ref& rhs);

    bool IsString() const;

    bool IsBool() const;

    bool IsDouble() const;

    bool IsInt() const;

    bool IsUInt() const;

    bool IsObject() const;

    bool IsNull() const;

    bool IsEmpty() const;

    bool IsArray() const;

    std::string AsString() const;

    Int AsInt() const;

    UInt AsUInt() const;

    bool AsBool() const;

    double AsDouble() const;

    operator JsonValue() const;

    Ref operator[](const char* key);

    Ref operator[](const std::string& key);

    Ref operator[](ArrayIndex index);

    const Ref operator[](const char* key) const;

    const Ref operator[](const std::string& key) const;

    const Ref operator[](ArrayIndex index) const;

    ArrayIndex Size() const;

    ValueType::Type Type() const;

    bool HasMember(const char* key) const;

    bool HasMember(const std::string& key) const;

    Members GetMemberNames() const;

    std::string ToJson() const;

    iterator begin();

    iterator end();

    const_iterator begin() const;

    const_iterator end() const;

    Ref Append(const JsonValue& value);

    void Clear();

   private:
    friend class JsonValue;
    friend class const_iterator;
    friend class iterator;

    enum Kind { None, Key, Index };

    explicit Ref(Storage& storage);

    Ref(Storage& storage, const char* key);

    Ref(Storage& storage, ArrayIndex index_);

    Ref(Storage& storage, ArrayIndex index_, Kind kind);

    Storage* storage_;

    Kind kind_;
  };

  friend class Ref;

  operator Ref();

  operator const Ref() const;

  Ref operator[](ArrayIndex index);

  const Ref operator[](ArrayIndex index) const;

  Ref operator[](const char* key);

  Ref operator[](const std::string& key);

  const Ref operator[](const char* key) const;

  const Ref operator[](const std::string& key) const;

  class iterator {
   public:
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef int32_t difference_type;
    typedef JsonValue value_type;
    typedef Ref reference;

    reference operator*() const;

    bool operator==(const iterator& other) const;

    bool operator!=(const iterator& other) const;

    bool operator==(const const_iterator& other) const;

    bool operator!=(const const_iterator& other) const;

    iterator& operator++();

    iterator operator++(difference_type);

    iterator& operator--();

    iterator operator--(difference_type);

    iterator operator+(difference_type j) const;

    iterator operator-(difference_type j) const;

    iterator& operator+=(difference_type j);

    iterator& operator-=(difference_type j);

   private:
    iterator();

    iterator(Storage* storage, ArrayIndex index, Ref::Kind kind);

    friend class const_iterator;
    friend class JsonValue;
    friend class Ref;

    Ref::Kind kind_;

    Storage* storage_;

    ArrayIndex index_;
  };

  friend class iterator;

  class const_iterator {
   public:
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef int32_t difference_type;
    typedef JsonValue value_type;
    typedef Ref reference;

    const reference operator*() const;

    bool operator==(const iterator& other) const;

    bool operator!=(const iterator& other) const;

    bool operator==(const const_iterator& other) const;

    bool operator!=(const const_iterator& other) const;

    const_iterator& operator++();

    const_iterator operator++(difference_type);

    const_iterator& operator--();

    const_iterator operator--(difference_type);

    const_iterator operator+(difference_type j) const;

    const_iterator operator-(difference_type j) const;

    const_iterator& operator+=(difference_type j);

    const_iterator& operator-=(difference_type j);

   private:
    const_iterator();

    const_iterator(const Storage* storage, ArrayIndex index, Ref::Kind kind);

    friend class iterator;
    friend class JsonValue;
    friend class Ref;

    Ref::Kind kind_;

    const Storage* storage_;

    ArrayIndex index_;
  };

  friend class const_iterator;

  static Ref::Kind GetKind(const ValueType::Type type);

  iterator begin();

  iterator end();

  const_iterator begin() const;

  const_iterator end() const;

  Ref Append(const JsonValue& value);

  void Clear();

 private:
  Storage storage_;
};

////////////////////////////////////////////////////////////////////////////////
/// class JsonValue::Ref
////////////////////////////////////////////////////////////////////////////////

inline JsonValue::Ref::Ref(JsonValue::Storage& storage)
    : storage_(&storage), kind_(None) {}

inline JsonValue::Ref& JsonValue::Ref::operator=(const JsonValue& rhs) {
  *storage_ = rhs.storage_;
  return *this;
}

inline JsonValue::Ref& JsonValue::Ref::operator=(const Ref& rhs) {
  *storage_ = *rhs.storage_;
  return *this;
}

inline JsonValue::Ref JsonValue::Ref::operator[](const char* key) {
  return Ref(*storage_, key);
}

inline JsonValue::Ref JsonValue::Ref::operator[](const std::string& key) {
  return (*this)[key.c_str()];
}

inline JsonValue::Ref JsonValue::Ref::operator[](
    utils::json::JsonValue::ArrayIndex index) {
  return Ref(*storage_, index);
}

inline const JsonValue::Ref JsonValue::Ref::operator[](
    utils::json::JsonValue::ArrayIndex index) const {
  return Ref(*storage_, index);
}

inline const JsonValue::Ref JsonValue::Ref::operator[](const char* key) const {
  return Ref(*storage_, key);
}

inline const JsonValue::Ref JsonValue::Ref::operator[](
    const std::string& key) const {
  return (*this)[key.c_str()];
}

inline JsonValue::Members JsonValue::Ref::GetMemberNames() const {
  return JsonValue(*storage_).GetMemberNames();
}

inline bool JsonValue::Ref::HasMember(const char* key) const {
  return JsonValue(*storage_).HasMember(key);
}

inline bool JsonValue::Ref::HasMember(const std::string& key) const {
  return HasMember(key.c_str());
}

inline std::string JsonValue::Ref::ToJson() const {
  return JsonValue(*storage_).ToJson();
}

inline JsonValue::Ref JsonValue::Ref::Append(const JsonValue& value) {
  return (*this)[Size()] = value;
}

inline JsonValue::ArrayIndex JsonValue::Ref::Size() const {
  return JsonValue(*storage_).Size();
}

inline ValueType::Type JsonValue::Ref::Type() const {
  return JsonValue(*storage_).Type();
}

inline bool JsonValue::Ref::IsString() const {
  return JsonValue(*storage_).IsString();
}

inline bool JsonValue::Ref::IsBool() const {
  return JsonValue(*storage_).IsBool();
}

inline bool JsonValue::Ref::IsDouble() const {
  return JsonValue(*storage_).IsDouble();
}

inline bool JsonValue::Ref::IsInt() const {
  return JsonValue(*storage_).IsInt();
}

inline bool JsonValue::Ref::IsUInt() const {
  return JsonValue(*storage_).IsUInt();
}

inline bool JsonValue::Ref::IsObject() const {
  return JsonValue(*storage_).IsObject();
}

inline bool JsonValue::Ref::IsNull() const {
  return JsonValue(*storage_).IsNull();
}

inline bool JsonValue::Ref::IsEmpty() const {
  return JsonValue(*storage_).IsEmpty();
}

inline bool JsonValue::Ref::IsArray() const {
  return JsonValue(*storage_).IsArray();
}

inline std::string JsonValue::Ref::AsString() const {
  return JsonValue(*storage_).AsString();
}

inline JsonValue::Int JsonValue::Ref::AsInt() const {
  return JsonValue(*storage_).AsInt();
}

inline JsonValue::UInt JsonValue::Ref::AsUInt() const {
  return JsonValue(*storage_).AsUInt();
}

inline bool JsonValue::Ref::AsBool() const {
  return JsonValue(*storage_).AsBool();
}

inline double JsonValue::Ref::AsDouble() const {
  return JsonValue(*storage_).AsDouble();
}

inline JsonValue::Ref::operator JsonValue() const {
  return JsonValue(*storage_);
}

inline JsonValue::iterator JsonValue::Ref::begin() {
  return iterator(storage_, 0, JsonValue::GetKind(Type()));
}

inline JsonValue::iterator JsonValue::Ref::end() {
  return iterator(storage_, Size(), JsonValue::GetKind(Type()));
}

inline JsonValue::const_iterator JsonValue::Ref::begin() const {
  return const_iterator(storage_, 0, JsonValue::GetKind(Type()));
}

inline JsonValue::const_iterator JsonValue::Ref::end() const {
  return const_iterator(storage_, Size(), JsonValue::GetKind(Type()));
}

////////////////////////////////////////////////////////////////////////////////
/// class JsonValue
////////////////////////////////////////////////////////////////////////////////

inline JsonValue::JsonValue(const char* value) : storage_(value) {}

inline JsonValue::JsonValue(Int value) : storage_(value) {}

inline JsonValue::JsonValue(UInt value) : storage_(value) {}

inline JsonValue::JsonValue(bool value) : storage_(value) {}

inline JsonValue::JsonValue(double value) : storage_(value) {}

inline JsonValue::JsonValue(const JsonValue::Storage& value)
    : storage_(value) {}

inline bool JsonValue::HasMember(const std::string& key) const {
  return HasMember(key.c_str());
}

inline JsonValue JsonValue::Get(const std::string& key,
                                const JsonValue& default_value) const {
  return Get(key.c_str(), default_value);
}

inline JsonValue::Ref JsonValue::operator[](ArrayIndex index) {
  return Ref(storage_, index);
}

inline const JsonValue::Ref JsonValue::operator[](ArrayIndex index) const {
  // It's safe to cast out since the result is const
  return Ref(const_cast<Storage&>(storage_), index);
}

inline JsonValue::Ref JsonValue::operator[](const char* key) {
  return Ref(storage_, key);
}

inline JsonValue::Ref JsonValue::operator[](const std::string& key) {
  return (*this)[key.c_str()];
}

inline const JsonValue::Ref JsonValue::operator[](const char* key) const {
  // It's safe to cast out since the result is const
  return Ref(const_cast<Storage&>(storage_), key);
}

inline const JsonValue::Ref JsonValue::operator[](
    const std::string& key) const {
  return (*this)[key.c_str()];
}

inline bool JsonValue::IsBool() const {
  return Type() == ValueType::BOOL_VALUE;
}

inline bool JsonValue::IsDouble() const {
  return Type() == ValueType::REAL_VALUE;
}

inline bool JsonValue::IsInt() const {
  return Type() == ValueType::INT_VALUE;
}

inline bool JsonValue::IsUInt() const {
  return Type() == ValueType::UINT_VALUE;
}

inline bool JsonValue::IsString() const {
  return Type() == ValueType::STRING_VALUE;
}

inline bool JsonValue::IsObject() const {
  return Type() == ValueType::OBJECT_VALUE;
}

inline bool JsonValue::IsNull() const {
  return Type() == ValueType::NULL_VALUE;
}

inline bool JsonValue::IsArray() const {
  return Type() == ValueType::ARRAY_VALUE;
}

inline JsonValue::iterator JsonValue::begin() {
  return iterator(&storage_, 0, JsonValue::GetKind(Type()));
}

inline JsonValue::iterator JsonValue::end() {
  return iterator(&storage_, Size(), JsonValue::GetKind(Type()));
}

inline JsonValue::const_iterator JsonValue::begin() const {
  return const_iterator(&storage_, 0, JsonValue::GetKind(Type()));
}

inline JsonValue::const_iterator JsonValue::end() const {
  return const_iterator(&storage_, Size(), JsonValue::GetKind(Type()));
}

inline JsonValue::operator JsonValue::Ref() {
  return Ref(storage_);
}

inline JsonValue::operator const JsonValue::Ref() const {
  // It's safe to cast out since the result is const
  return Ref(const_cast<Storage&>(storage_));
}

inline JsonValue::Ref JsonValue::Append(const JsonValue& value) {
  return (*this)[Size()] = value;
}

////////////////////////////////////////////////////////////////////////////////
/// class JsonValue::iterator
////////////////////////////////////////////////////////////////////////////////

inline JsonValue::iterator::iterator()
    : storage_(NULL), index_(0), kind_(Ref::None) {}

inline JsonValue::iterator::iterator(JsonValue::Storage* storage,
                                     JsonValue::ArrayIndex index,
                                     Ref::Kind kind)
    : storage_(storage), index_(index), kind_(kind) {}

inline JsonValue::iterator::reference JsonValue::iterator::operator*() const {
  return Ref(*storage_, index_, kind_);
}

inline bool JsonValue::iterator::operator==(
    const JsonValue::iterator& other) const {
  return index_ == other.index_;
}

inline bool JsonValue::iterator::operator!=(
    const JsonValue::iterator& other) const {
  return index_ != other.index_;
}

inline bool JsonValue::iterator::operator==(
    const JsonValue::const_iterator& other) const {
  return index_ == other.index_;
}

inline bool JsonValue::iterator::operator!=(
    const JsonValue::const_iterator& other) const {
  return index_ != other.index_;
}

inline JsonValue::iterator& JsonValue::iterator::operator++() {
  ++index_;
  return *this;
}

inline JsonValue::iterator JsonValue::iterator::operator++(
    JsonValue::iterator::difference_type) {
  iterator result = *this;
  ++index_;
  return result;
}

inline JsonValue::iterator& JsonValue::iterator::operator--() {
  --index_;
  return *this;
}

inline JsonValue::iterator JsonValue::iterator::operator--(
    JsonValue::iterator::difference_type) {
  iterator result = *this;
  --index_;
  return result;
}

inline JsonValue::iterator JsonValue::iterator::operator+(
    JsonValue::iterator::difference_type j) const {
  iterator result = *this;
  result.index_ += j;
  return result;
}

inline JsonValue::iterator JsonValue::iterator::operator-(
    JsonValue::iterator::difference_type j) const {
  return operator+(-j);
}

inline JsonValue::iterator& JsonValue::iterator::operator+=(
    JsonValue::iterator::difference_type j) {
  index_ += j;
  return *this;
}

inline JsonValue::iterator& JsonValue::iterator::operator-=(
    JsonValue::iterator::difference_type j) {
  index_ -= j;
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// class JsonValue::const_iterator
////////////////////////////////////////////////////////////////////////////////

inline JsonValue::const_iterator::const_iterator()
    : storage_(NULL), index_(0), kind_(Ref::None) {}

inline JsonValue::const_iterator::const_iterator(
    const JsonValue::Storage* storage, ArrayIndex index, Ref::Kind kind)
    : storage_(storage), index_(index), kind_(kind) {}

inline const JsonValue::const_iterator::reference JsonValue::const_iterator::
operator*() const {
  // Result is const thus it's safe to cact out the const
  return Ref(const_cast<Storage&>(*storage_), index_, kind_);
}

inline bool JsonValue::const_iterator::operator==(
    const JsonValue::iterator& other) const {
  return index_ == other.index_;
}

inline bool JsonValue::const_iterator::operator!=(
    const JsonValue::iterator& other) const {
  return index_ != other.index_;
}

inline bool JsonValue::const_iterator::operator==(
    const JsonValue::const_iterator& other) const {
  return index_ == other.index_;
}

inline bool JsonValue::const_iterator::operator!=(
    const JsonValue::const_iterator& other) const {
  return index_ != other.index_;
}

inline JsonValue::const_iterator& JsonValue::const_iterator::operator++() {
  ++index_;
  return *this;
}

inline JsonValue::const_iterator JsonValue::const_iterator::operator++(
    JsonValue::const_iterator::difference_type) {
  const_iterator result = *this;
  ++index_;
  return result;
}

inline JsonValue::const_iterator& JsonValue::const_iterator::operator--() {
  --index_;
  return *this;
}

inline JsonValue::const_iterator JsonValue::const_iterator::operator--(
    JsonValue::const_iterator::difference_type) {
  const_iterator result = *this;
  --index_;
  return result;
}

inline JsonValue::const_iterator JsonValue::const_iterator::operator+(
    JsonValue::const_iterator::difference_type j) const {
  const_iterator result = *this;
  result.index_ += j;
  return result;
}

inline JsonValue::const_iterator JsonValue::const_iterator::operator-(
    JsonValue::const_iterator::difference_type j) const {
  return operator+(-j);
}

inline JsonValue::const_iterator& JsonValue::const_iterator::operator+=(
    JsonValue::const_iterator::difference_type j) {
  index_ += j;
  return *this;
}

inline JsonValue::const_iterator& JsonValue::const_iterator::operator-=(
    JsonValue::const_iterator::difference_type j) {
  index_ -= j;
  return *this;
}

}  // namespace json
}  // namespace utils

#endif  // SRC_COMPONENTS_UTILS_INCLUDE_UTILS_JSON_UTILS_H_
