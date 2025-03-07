// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
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

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// Defines the abstract interface implemented by each of the language-specific
// code generators.

#ifndef GOOGLE_PROTOBUF_COMPILER_CODE_GENERATOR_H__
#define GOOGLE_PROTOBUF_COMPILER_CODE_GENERATOR_H__

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/compiler/retention.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/port.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {

namespace io {
class ZeroCopyOutputStream;
}
class FileDescriptor;
class GeneratedCodeInfo;

namespace compiler {
class AccessInfoMap;

class Version;

// Defined in this file.
class CodeGenerator;
class GeneratorContext;

// The abstract interface to a class which generates code implementing a
// particular proto file in a particular language.  A number of these may
// be registered with CommandLineInterface to support various languages.
class PROTOC_EXPORT CodeGenerator {
 public:
  CodeGenerator() {}
  CodeGenerator(const CodeGenerator&) = delete;
  CodeGenerator& operator=(const CodeGenerator&) = delete;
  virtual ~CodeGenerator();

  // Generates code for the given proto file, generating one or more files in
  // the given output directory.
  //
  // A parameter to be passed to the generator can be specified on the command
  // line. This is intended to be used to pass generator specific parameters.
  // It is empty if no parameter was given. ParseGeneratorParameter (below),
  // can be used to accept multiple parameters within the single parameter
  // command line flag.
  //
  // Returns true if successful.  Otherwise, sets *error to a description of
  // the problem (e.g. "invalid parameter") and returns false.
  virtual bool Generate(const FileDescriptor* file,
                        const std::string& parameter,
                        GeneratorContext* generator_context,
                        std::string* error) const = 0;

  // Generates code for all given proto files.
  //
  // WARNING: The canonical code generator design produces one or two output
  // files per input .proto file, and we do not wish to encourage alternate
  // designs.
  //
  // A parameter is given as passed on the command line, as in |Generate()|
  // above.
  //
  // Returns true if successful.  Otherwise, sets *error to a description of
  // the problem (e.g. "invalid parameter") and returns false.
  virtual bool GenerateAll(const std::vector<const FileDescriptor*>& files,
                           const std::string& parameter,
                           GeneratorContext* generator_context,
                           std::string* error) const;

  // This must be kept in sync with plugin.proto. See that file for
  // documentation on each value.
  // TODO(b/291092901) Use CodeGeneratorResponse.Feature here.
  enum Feature {
    FEATURE_PROTO3_OPTIONAL = 1,
    FEATURE_SUPPORTS_EDITIONS = 2,
  };

  // Implement this to indicate what features this code generator supports.
  //
  // This must be a bitwise OR of values from the Feature enum above (or zero).
  virtual uint64_t GetSupportedFeatures() const { return 0; }

  // This is no longer used, but this class is part of the opensource protobuf
  // library, so it has to remain to keep vtables the same for the current
  // version of the library. When protobufs does a api breaking change, the
  // method can be removed.
  virtual bool HasGenerateAll() const { return true; }

  // Returns all the feature extensions used by this generator.  This must be in
  // the generated pool, meaning that the extensions should be linked into this
  // binary.  Any generator features not included here will not get properly
  // resolved and GetResolvedSourceFeatures will not provide useful values.
  virtual std::vector<const FieldDescriptor*> GetFeatureExtensions() const {
    return {};
  }

  // Returns the minimum edition (inclusive) supported by this generator.  Any
  // proto files with an edition before this will result in an error.
  virtual Edition GetMinimumEdition() const { return PROTOBUF_MINIMUM_EDITION; }

  // Returns the maximum edition (inclusive) supported by this generator.  Any
  // proto files with an edition after this will result in an error.
  virtual Edition GetMaximumEdition() const { return PROTOBUF_MAXIMUM_EDITION; }

  // Builds a default feature set mapping for this generator.
  //
  // This will use the extensions specified by GetFeatureExtensions(), with the
  // supported edition range [GetMinimumEdition(), GetMaximumEdition].  It has
  // no side-effects, and code generators only need to call this if they want to
  // embed the defaults into the generated code.
  absl::StatusOr<FeatureSetDefaults> BuildFeatureSetDefaults() const;

 protected:
  // Retrieves the resolved source features for a given descriptor.  All the
  // features that are imported (from the proto file) and linked in (from the
  // callers binary) will be fully resolved. These should be used to make any
  // feature-based decisions during code generation.
  template <typename DescriptorT>
  static const FeatureSet& GetResolvedSourceFeatures(const DescriptorT& desc) {
    return ::google::protobuf::internal::InternalFeatureHelper::GetFeatures(desc);
  }

  // Retrieves the unresolved source features for a given descriptor.  These
  // should be used to validate the original .proto file.  These represent the
  // original proto files from generated code, but should be stripped of
  // source-retention features before sending to a runtime.
  template <typename DescriptorT, typename TypeTraitsT, uint8_t field_type,
            bool is_packed>
  static typename TypeTraitsT::ConstType GetUnresolvedSourceFeatures(
      const DescriptorT& descriptor,
      const google::protobuf::internal::ExtensionIdentifier<
          FeatureSet, TypeTraitsT, field_type, is_packed>& extension) {
    return ::google::protobuf::internal::InternalFeatureHelper::GetUnresolvedFeatures(
        descriptor, extension);
  }
};

// CodeGenerators generate one or more files in a given directory.  This
// abstract interface represents the directory to which the CodeGenerator is
// to write and other information about the context in which the Generator
// runs.
class PROTOC_EXPORT GeneratorContext {
 public:
  GeneratorContext() {
  }
  GeneratorContext(const GeneratorContext&) = delete;
  GeneratorContext& operator=(const GeneratorContext&) = delete;
  virtual ~GeneratorContext();

  // Opens the given file, truncating it if it exists, and returns a
  // ZeroCopyOutputStream that writes to the file.  The caller takes ownership
  // of the returned object.  This method never fails (a dummy stream will be
  // returned instead).
  //
  // The filename given should be relative to the root of the source tree.
  // E.g. the C++ generator, when generating code for "foo/bar.proto", will
  // generate the files "foo/bar.pb.h" and "foo/bar.pb.cc"; note that
  // "foo/" is included in these filenames.  The filename is not allowed to
  // contain "." or ".." components.
  virtual io::ZeroCopyOutputStream* Open(const std::string& filename) = 0;

  // Similar to Open() but the output will be appended to the file if exists
  virtual io::ZeroCopyOutputStream* OpenForAppend(const std::string& filename);

  // Creates a ZeroCopyOutputStream which will insert code into the given file
  // at the given insertion point.  See plugin.proto (plugin.pb.h) for more
  // information on insertion points.  The default implementation
  // assert-fails -- it exists only for backwards-compatibility.
  //
  // WARNING:  This feature is currently EXPERIMENTAL and is subject to change.
  virtual io::ZeroCopyOutputStream* OpenForInsert(
      const std::string& filename, const std::string& insertion_point);

  // Similar to OpenForInsert, but if `info` is non-empty, will open (or create)
  // filename.pb.meta and insert info at the appropriate place with the
  // necessary shifts. The default implementation ignores `info`.
  //
  // WARNING:  This feature will be REMOVED in the near future.
  virtual io::ZeroCopyOutputStream* OpenForInsertWithGeneratedCodeInfo(
      const std::string& filename, const std::string& insertion_point,
      const google::protobuf::GeneratedCodeInfo& info);

  // Returns a vector of FileDescriptors for all the files being compiled
  // in this run.  Useful for languages, such as Go, that treat files
  // differently when compiled as a set rather than individually.
  virtual void ListParsedFiles(std::vector<const FileDescriptor*>* output);

  // Retrieves the version number of the protocol compiler associated with
  // this GeneratorContext.
  virtual void GetCompilerVersion(Version* version) const;

};

// The type GeneratorContext was once called OutputDirectory. This typedef
// provides backward compatibility.
typedef GeneratorContext OutputDirectory;

// Several code generators treat the parameter argument as holding a
// list of options separated by commas.  This helper function parses
// a set of comma-delimited name/value pairs: e.g.,
//   "foo=bar,baz,moo=corge"
// parses to the pairs:
//   ("foo", "bar"), ("baz", ""), ("moo", "corge")
PROTOC_EXPORT void ParseGeneratorParameter(
    absl::string_view, std::vector<std::pair<std::string, std::string> >*);

// Strips ".proto" or ".protodevel" from the end of a filename.
PROTOC_EXPORT std::string StripProto(absl::string_view filename);

}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_COMPILER_CODE_GENERATOR_H__
