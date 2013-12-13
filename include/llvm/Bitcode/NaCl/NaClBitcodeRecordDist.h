//===- NaClBitcodeRecordDist.h -----------------------------------*- C++ -*-===//
//     Maps distributions of values in PNaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Creates a (nestable) distribution map of values in PNaCl bitcode
// records. The domain of these maps is the set of record values being
// tracked. The range is the information associated with each record
// value, including the number of instances of that value. The
// distribution map is nested if the range element contains another
// distribution map.
//
// The goal of distribution maps is to build a (histogram)
// distribution of values in bitcode records, of a PNaCl bitcode
// file. From appropriately built distribution maps, one can infer
// possible new abbreviations that can be used in the PNaCl bitcode
// file.  Hence, one of the primary goals of distribution maps is to
// support tools pnacl-bcanalyzer and pnacl-bccompress.
//
// To make the API uniform, distribution maps are updated using
// NaClBitcodeRecords (in NaClBitcodeParser.h). The values from the
// record are defined by the extract method GetValueList, and added
// via Method Add.  This same API makes handling of nested
// distribution maps easy by allowing the nested map to extract the
// values it needs, for the distribution it is modeling, independent
// of the distribution map it appears in.
//
// In addition, there is the virtual method CreateElement that
// creates a new range element in the distribution map. This
// allows the distribution map to do two things:
//
// 1) Add any additional information needed by the element, based
//    on the distribution map.
//
// 2) Add a nested distribution map to the created element if
// appropriate.
//
// Distribution maps are sortable (via method GetDistribution). The
// purpose of sorting is to find interesting elements. This is done by
// sorting the values in the domain of the distribution map, based on
// the GetImportance method of the range element.
//
// Method GetImportance defines how (potentially) interesting the
// value is in the distribution. "Interesting" is based on the notion
// of how likely will the value show a case where adding an
// abbreviation will shrink the size of the corresponding bitcode
// file. For most distributions, the number of instances associated
// with the value is the best measure.
//
// However, for cases where multiple domain entries are created for
// the same NaClBitcodeRecord (i.e. method GetValueList defines more
// than one value), things are not so simple.

// However, for some distributions (such as value index distributions)
// the numbers of instances isn't sufficient. In such cases, you may
// have to look at nested distributions to find important cases.
//
// In the case of value index distributions, when the size of the
// records is the same, all value indices have the same number of
// instances.  In this case, "interesting" may be measured in terms of
// the (nested) distribution of the values that can appear at that
// index, and how often each value appears.
//
// The larger the importance, the more interesting the value is
// considered, and sorting is based on moving interesting values to
// the front of the sorted list.
//
// When printing distribution maps, the values are sorted based on
// the importance. By default, importance is based on the number of
// times the value appears in records, putting the most used values
// at the top of the printed list.
//
// Since sorting is expensive, the sorted distribution is built once
// and cached.  This cache is flushed whenever the distribution map is
// updated, so that a new sorted distribuition will be generated.
//
// Printing of distribution maps are stylized, so that virtuals can
// easily fill in the necessary data. Each distribution map (nested
// or top-level) has a title, that is retrieved from method GetTitle,
// and is printed first.
//
// Then, a header (showing what each column in the printed histogram
// includes) is printed. This header is generated by method
// PrintHeader.  In addition, the (domain) value of the histogram is
// always printed as the last element in a row, and the header
// descriptor for this value is provided by method GetValueHeader.
//
// After the header, rows of the (sorted) distribution map are
// printed.  Each row contains a value and a sequence of statistics
// based on the corresponding range element. To allow the printing of
// (optional) nested distributions, The statistics are printed first,
// followed by the value. Method PrintRowStats prints the statistics
// of the range element, and PrintRowValue prints the corresponding
// (domain) value. Unless PrintRowValue is overridden, this method
// uses a format string that will right justify the value based on the
// length of the header name (the value returned by GetValueHeader).
//
// If the range element contains a nested distribution map, it is then
// printed below that row, indented further than the current
// distribution map.
//
// Distribution maps are implemented as subclasses of the class
// NaClBitcodeRecordDist, whose domain type is
// NaClBitcodeRecordDistValue, and range elements are subclasses of
// the class NaClBitcodeRecordDistElement.
//
// Simple (non-nested) distribution maps must, at a minimum define
// method GetValueList, to extract values out of the bitcode record.
// In addition, only if the range element needs a non-default
// constructor, one must override the method CreateElement to call the
// appropriate constructor with the appropriate arguments.
//
// Nested distribution maps are created by defining a derived class of
// another distribution map. This derived class must implement
// CreateNestedDistributionMap, which returns the corresponding
// (dynamically) allocated nested distribution map to be associated
// with the element created by CreateElement.

#ifndef LLVM_BITCODE_NACL_NACLBITCODERECORDDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODERECORDDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include <map>
#include <vector>

namespace llvm {

/// The domain type of PNaCl bitcode record distribution maps.
typedef uint64_t NaClBitcodeRecordDistValue;

/// Base class of the range type of PNaCl bitcode record distribution
/// maps.
class NaClBitcodeRecordDistElement;

/// Type defining the list of values extracted from the corresponding
/// bitcode record. Typically, the list size is one. However, there
/// are cases where a record defines more than one value (i.e. value
/// indices). Hence, this type defines the more generic API for
/// values.
typedef std::vector<NaClBitcodeRecordDistValue> ValueListType;

typedef ValueListType::const_iterator ValueListIterator;

/// Defines a PNaCl bitcode record distribution map. The distribution
/// map is a map from a (record) value, to the corresponding data
/// associated with that value.
///
/// ElementType is assumed to be a derived class of
/// NaClBitcodeRecordDistElement.
template<class ElementType>
class NaClBitcodeRecordDist {
  NaClBitcodeRecordDist(const NaClBitcodeRecordDist<ElementType>&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordDist<ElementType>&)
      LLVM_DELETED_FUNCTION;

public:
  /// Type defining the mapping used to define the distribution.
  typedef typename
  std::map<NaClBitcodeRecordDistValue, ElementType*> MappedElement;

  typedef typename MappedElement::const_iterator const_iterator;

  /// Type defining a pair of values used to sort the
  /// distribution. The first element is defined by method
  /// GetImportance, and the second is the distribution value
  /// associated with that importance.
  typedef std::pair<double, NaClBitcodeRecordDistValue> DistPair;

  /// Type defining the sorted list of (domain) values in the
  /// corresponding distribution map.
  typedef std::vector<DistPair> Distribution;

  NaClBitcodeRecordDist()
      : TableMap(), CachedDistribution(0), Total(0) {
  }

  virtual ~NaClBitcodeRecordDist() {
    RemoveCachedDistribution();
    for (const_iterator Iter = begin(), IterEnd = end();
         Iter != IterEnd; ++Iter) {
      delete Iter->second;
    }
  }

  /// Number of elements in the distribution map.
  size_t size() const {
    return TableMap.size();
  }

  /// Iterator at beginning of distribution map.
  const_iterator begin() const {
    return TableMap.begin();
  }

  /// Iterator at end of distribution map.
  const_iterator end() const {
    return TableMap.end();
  }

  /// Returns true if the distribution map is empty.
  bool empty() const {
    return TableMap.empty();
  }

  /// Returns the element associated with the given distribution
  /// value.  Creates the element if needed.
  ElementType *GetElement(NaClBitcodeRecordDistValue Value) {
    if (TableMap.find(Value) == TableMap.end()) {
      TableMap[Value] = CreateElement(Value);
    }
    return TableMap[Value];
  }

  /// Returns the element associated with the given distribution
  /// value.
  ElementType *at(NaClBitcodeRecordDistValue Value) const {
    return TableMap.at(Value);
  }

  /// Returns the total number of instances held in the distribution
  /// map.
  unsigned GetTotal() const {
    return Total;
  }

  /// Adds the value(s) in the given bitcode record to the
  /// distribution map.  The value(s) based on method GetValueList.
  virtual void Add(const NaClBitcodeRecord &Record) {
    ValueListType ValueList;
    this->GetValueList(Record, ValueList);
    if (!ValueList.empty()) {
      RemoveCachedDistribution();
      ++Total;
      for (ValueListIterator
               Iter = ValueList.begin(),
               IterEnd = ValueList.end();
           Iter != IterEnd; ++Iter) {
        GetElement(*Iter)->Add(Record);
      }
    }
  }

  /// Builds the distribution associated with the distribution map.
  /// Warning: The distribution is cached, and hence, only valid while
  /// it's contents is not changed.
  Distribution *GetDistribution() const {
    if (CachedDistribution == 0) Sort();
    return CachedDistribution;
  }

  /// Prints out the contents of the distribution map to Stream.
  void Print(raw_ostream &Stream, std::string Indent="") const {
    Distribution *Dist = this->GetDistribution();
    PrintTitle(Stream, Indent);
    PrintHeader(Stream, Indent);
    for (size_t I = 0, E = Dist->size(); I != E; ++I) {
      const DistPair &Pair = Dist->at(I);
      PrintRow(Stream, Indent, Pair.second);
    }
  }

protected:
  /// Creates a distribution element for the given value.
  virtual ElementType *CreateElement(NaClBitcodeRecordDistValue Value) {
    return new ElementType(CreateNestedDistributionMap());
  }

  /// Returns the (optional) nested distribution map to be associated
  // with the element. Returning 0 implies that no nested distribution map
  // will be added to the element.
  virtual NaClBitcodeRecordDist<NaClBitcodeRecordDistElement>*
  CreateNestedDistributionMap() {
    return 0;
  }

  /// If the distribution is cached, remove it. Should be called
  /// whenever the distribution map is changed.
  void RemoveCachedDistribution() const {
    if (CachedDistribution) {
      delete CachedDistribution;
      CachedDistribution = 0;
    }
  }

  /// Interrogates the block record, and returns the corresponding
  /// values that are being tracked by the distribution map. Must be
  /// defined in derived classes.
  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const = 0;

  /// Returns the title to use when printing the distribution map.
  virtual const char *GetTitle() const {
    return "Distribution";
  }

  /// Returns the header to use when printing the value in the
  /// distribution map.
  virtual const char *GetValueHeader() const {
    return "Value";
  }

  /// Prints out the title of the distribution map.
  virtual void PrintTitle(raw_ostream &Stream, std::string Indent) const {
    Stream << Indent << GetTitle() << " (" << size() << " elements):\n\n";
  }

  /// Prints out statistics for the row with the given value.
  virtual void PrintRowStats(raw_ostream &Stream,
                             std::string Indent,
                             NaClBitcodeRecordDistValue Value) const {
    Stream << Indent << format("%7d ", at(Value)->GetNumInstances()) << "    ";
  }

  /// Prints out Value (in a row) to Stream. If the element contains a
  /// nested distribution, that nested distribution will use the given
  /// Indent for this distribution to properly indent the nested
  /// distribution.
  virtual void PrintRowValue(raw_ostream &Stream,
                             std::string Indent,
                             NaClBitcodeRecordDistValue Value) const {
    std::string ValueFormat;
    raw_string_ostream StrStream(ValueFormat);
    StrStream << "%" << strlen(GetValueHeader()) << "d";
    StrStream.flush();
    Stream << format(ValueFormat.c_str(), (int) Value);
    // TODO(kschimpf): Print nested distribution here if applicable.
    // Note: Indent would be used in this context.
  }

  // Prints out the header to the printed distribution map.
  virtual void PrintHeader(raw_ostream &Stream, std::string Indent) const {
    Stream << Indent << "  Count     " << GetValueHeader() << "\n";
  }

  // Prints out a row in the printed distribution map.
  virtual void PrintRow(raw_ostream &Stream,
                        std::string Indent,
                        NaClBitcodeRecordDistValue Value) const {
    PrintRowStats(Stream, Indent, Value);
    PrintRowValue(Stream, Indent, Value);
    Stream << "\n";
  }

  /// Sorts the distribution, based on the importance of each element.
  void Sort() const {
    RemoveCachedDistribution();
    CachedDistribution = new Distribution();
    for (const_iterator Iter = begin(), IterEnd = end();
         Iter != IterEnd; ++Iter) {
      const ElementType *Elmt = Iter->second;
      if (double Importance = Elmt->GetImportance())
        CachedDistribution->push_back(std::make_pair(Importance, Iter->first));
    }
    // Sort in ascending order, based on importance.
    std::stable_sort(CachedDistribution->begin(),
                     CachedDistribution->end());
    // Reverse so most important appear first.
    std::reverse(CachedDistribution->begin(),
                 CachedDistribution->end());
  }

private:
  // Map from the distribution value to the corresponding distribution
  // element.
  MappedElement TableMap;
  // Pointer to the cached distribution.
  mutable Distribution *CachedDistribution;
  // The total number of instances in the map.
  unsigned Total;
};

/// Defines the element type of a PNaCl bitcode distribution map.
/// This is the base class for all element types used in
/// NaClBitcodeRecordDist.  By default, only the number of instances
/// of the corresponding distribution values is recorded.
class NaClBitcodeRecordDistElement {
  NaClBitcodeRecordDistElement(const NaClBitcodeRecordDistElement &)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordDistElement &)
      LLVM_DELETED_FUNCTION;

public:
  // Create an element with no instances.
  NaClBitcodeRecordDistElement(
      NaClBitcodeRecordDist<NaClBitcodeRecordDistElement>* NestedDist)
      : NestedDist(NestedDist), NumInstances(0)
  {}

  virtual ~NaClBitcodeRecordDistElement() {
    delete NestedDist;
  }

  // Adds an instance of the given record to this instance.
  virtual void Add(const NaClBitcodeRecord &Record) {
    if (NestedDist) NestedDist->Add(Record);
    ++NumInstances;
  }

  // Returns the number of instances associated with this element.
  unsigned GetNumInstances() const {
    return NumInstances;
  }

  // Returns the importance of this element, and the number of
  // instances associated with it. Used to sort the distribution map,
  // where values with larger importance appear first.
  virtual double GetImportance() const {
    return static_cast<double>(NumInstances);
  }

protected:
  // The (optional) nested distribution.
  NaClBitcodeRecordDist<NaClBitcodeRecordDistElement> *NestedDist;

private:
  // The number of instances associated with this element.
  unsigned NumInstances;
};

}

#endif
