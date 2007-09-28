// Copyright 2007, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifdef WIN32
#include <windows.h>
#endif

#include "googleurl/src/gurl.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "googleurl/src/url_canon_stdstring.h"
#include "googleurl/src/url_util.h"

namespace {

// External template that can handle initialization of either character type.
// The input spec is given, and the canonical version will be placed in
// |*canonical|, along with the parsing of the canonical spec in |*parsed|.
template<typename CHAR>
bool InitCanonical(const std::basic_string<CHAR>& input_spec,
                   std::string* canonical,
                   url_parse::Parsed* parsed) {
  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  canonical->reserve(input_spec.size() + 32);
  url_canon::StdStringCanonOutput output(canonical);
  bool success = url_util::Canonicalize(
      input_spec.data(), static_cast<int>(input_spec.length()),
      &output, parsed);

  output.Complete();  // Must be done before using string.
  return success;
}

} // namespace

GURL::GURL() : is_valid_(false) {
}

GURL::GURL(const GURL& other)
    : is_valid_(other.is_valid_),
      spec_(other.spec_),
      parsed_(other.parsed_) {
}

GURL::GURL(const std::string& url_string) {
  is_valid_ = InitCanonical(url_string, &spec_, &parsed_);
}

GURL::GURL(const std::wstring& url_string) {
  is_valid_ = InitCanonical(url_string, &spec_, &parsed_);
}

GURL::GURL(const char* canonical_spec, int canonical_spec_len,
           const url_parse::Parsed& parsed, bool is_valid)
    : is_valid_(is_valid),
      spec_(canonical_spec, canonical_spec_len),
      parsed_(parsed) {
#ifndef NDEBUG
  // For testing purposes, check that the parsed canonical URL is
  // identical to what we would have produced.
  GURL test_url(spec_);

  DCHECK(test_url.is_valid_ == is_valid_);
  DCHECK(test_url.spec_ == spec_);

  DCHECK(test_url.parsed_.scheme == parsed_.scheme);
  DCHECK(test_url.parsed_.username == parsed_.username);
  DCHECK(test_url.parsed_.password == parsed_.password);
  DCHECK(test_url.parsed_.host == parsed_.host);
  DCHECK(test_url.parsed_.port == parsed_.port);
  DCHECK(test_url.parsed_.path == parsed_.path);
  DCHECK(test_url.parsed_.query == parsed_.query);
  DCHECK(test_url.parsed_.ref == parsed_.ref);
#endif
}

const std::string& GURL::spec() const {
  if (is_valid_ || spec_.empty())
    return spec_;

  NOTREACHED() << "Trying to get the spec of an invalid URL!";
  return EmptyString();
}

// Note: code duplicated below (it's inconvenient to use a template here).
GURL GURL::Resolve(const std::string& relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  url_canon::StdStringCanonOutput output(&result.spec_);

  if (!url_util::ResolveRelative(spec_.data(), parsed_, relative.data(),
                                 static_cast<int>(relative.length()),
                                 &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  return result;
}

// Note: code duplicated above (it's inconvenient to use a template here).
GURL GURL::Resolve(const std::wstring& relative) const {
  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  GURL result;

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  url_canon::StdStringCanonOutput output(&result.spec_);

  if (!url_util::ResolveRelative(spec_.data(), parsed_, relative.data(),
                                 static_cast<int>(relative.length()),
                                 &output, &result.parsed_)) {
    // Error resolving, return an empty URL.
    return GURL();
  }

  output.Complete();
  result.is_valid_ = true;
  return result;
}

GURL GURL::ReplaceComponents(const Replacements& replacements) const {
  GURL result;

  // Not allowed for invalid URLs.
  if (!is_valid_)
    return GURL();

  // Reserve enough room in the output for the input, plus some extra so that
  // we have room if we have to escape a few things without reallocating.
  result.spec_.reserve(spec_.size() + 32);
  url_canon::StdStringCanonOutput output(&result.spec_);

  result.is_valid_ = url_util::ReplaceComponents(
      spec_.data(), static_cast<int>(spec_.size()), parsed_, replacements,
      &output, &result.parsed_);

  output.Complete();
  return result;
}

/* TODO(brettw) support wide replacements!
GURL GURL::ReplaceComponents(const ReplacementsW& replacements) const {
}
*/

GURL GURL::GetWithEmptyPath() const {
  // This doesn't make sense for invalid or nonstandard URLs, so return
  // the empty URL.
  if (!is_valid_ || !SchemeIsStandard()) 
    return GURL();

  // We could optimize this since we know that the URL is canonical, and we are
  // appending a canonical path, so avoiding re-parsing.
  GURL other(*this);
  if (parsed_.path.len == 0)
    return other;

  // Clear everything after the path.
  other.parsed_.query.reset();
  other.parsed_.ref.reset();

  // Set the path, since the path is longer than one, we can just set the
  // first character and resize.
  other.spec_[other.parsed_.path.begin] = '/';
  other.parsed_.path.len = 1;
  other.spec_.resize(other.parsed_.path.begin + 1);
  return other;
}

bool GURL::SchemeIs(const char* lower_ascii_scheme) const {
  if (parsed_.scheme.len <= 0)
    return lower_ascii_scheme == NULL;
  return LowerCaseEqualsASCII(spec_.begin() + parsed_.scheme.begin,
                              spec_.begin() + parsed_.scheme.end(),
                              lower_ascii_scheme);
}

bool GURL::SchemeIsStandard() const {
  return url_util::IsStandardScheme(&spec_[parsed_.scheme.begin],
                                    parsed_.scheme.len);
}

int GURL::IntPort() const {
  if (parsed_.port.is_nonempty())
    return url_parse::ParsePort(spec_.data(), parsed_.port);
  return url_parse::PORT_UNSPECIFIED;
}

std::string GURL::ExtractFileName() const {
  url_parse::Component file_component;
  url_parse::ExtractFileName(spec_.data(), parsed_.path, &file_component);
  return ComponentString(file_component);
}

void GURL::ExtractQuery(QueryMap* r) const {
  url_parse::Component q(parsed_.query);
  url_parse::Component key;
  url_parse::Component value;

  while (q.is_nonempty()) {
    key = url_parse::Component();
    value = url_parse::Component();

    url_parse::ExtractQueryFragment(spec_.data(),
                                    &q,
                                    &key,
                                    &value);

    // Something may or may not have been found. For example, the key and value
    // will both be empty for the query string "&".
    if (key.len > 0) {
      if (value.len > 0)
        (*r)[ComponentString(key)] = ComponentString(value);
      else
        (*r)[ComponentString(key)] = std::string("");
    }
  }
}

std::string GURL::PathForRequest() const {
  DCHECK(parsed_.path.len > 0) << "Canonical path for requests should be non-empty";
  if (parsed_.ref.len >= 0) {
    // Clip off the reference when it exists. The reference starts after the #
    // sign, so we have to subtract one to also remove it.
    return std::string(spec_, parsed_.path.begin,
                       parsed_.ref.begin - parsed_.path.begin - 1);
  }

  // Use everything form the path to the end.
  return std::string(spec_, parsed_.path.begin);
}

bool GURL::HostIsIPAddress() const {
  if (!is_valid_ || spec_.empty())
     return false;

  url_canon::RawCanonOutputT<char, 128> ignored_output;
  url_parse::Component ignored_component;
  return url_canon::CanonicalizeIPAddress(spec_.c_str(), parsed_.host,
                                          &ignored_output, &ignored_component);
}

#ifdef WIN32

// static
const GURL& GURL::EmptyGURL() {
  // Avoid static object construction/destruction on startup/shutdown.
  static GURL* empty_gurl = NULL;
  if (!empty_gurl) {
    // Create the string. Be careful that we don't break in the case that this
    // is being called from multiple threads.
    GURL* new_empty_gurl = new GURL;
    if (InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&empty_gurl), new_empty_gurl, NULL)) {
      // The old value was non-NULL, so no replacement was done. Another
      // thread did the initialization out from under us.
      delete new_empty_gurl;
    }
  }
  return *empty_gurl;
}

#endif  // WIN32
