/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ORC_STATISTICS_IMPL_HH
#define ORC_STATISTICS_IMPL_HH

#include "orc/Common.hh"
#include "orc/Int128.hh"
#include "orc/OrcFile.hh"
#include "orc/Reader.hh"

#include "Timezone.hh"
#include "TypeImpl.hh"

namespace orc {

/**
 * StatContext contains fields required to compute statistics
 */

  struct StatContext {
    const bool correctStats;
    const Timezone* const writerTimezone;
    StatContext() : correctStats(false), writerTimezone(NULL) {}
    StatContext(bool cStat, const Timezone* const timezone = NULL) :
        correctStats(cStat), writerTimezone(timezone) {}
  };

/**
 * Internal Statistics Implementation
 */

  template <typename T>
  class InternalStatisticsImpl {
  private:
    bool _hasNull;
    bool _hasMinimum;
    bool _hasMaximum;
    bool _hasSum;
    bool _hasTotalLength;
    uint64_t _totalLength;
    uint64_t _valueCount;
    T _minimum;
    T _maximum;
    T _sum;
  public:
    InternalStatisticsImpl() {
      _hasNull = false;
      _hasMinimum = false;
      _hasMaximum = false;
      _hasSum = false;
      _hasTotalLength = false;
      _totalLength = 0;
      _valueCount = 0;
    }

    ~InternalStatisticsImpl() {}

    // GET / SET _totalLength
    bool hasTotalLength() const { return _hasTotalLength; }

    void setHasTotalLength(bool hasTotalLength) { _hasTotalLength = hasTotalLength; }

    uint64_t getTotalLength() const { return _totalLength; }

    void setTotalLength(uint64_t totalLength) { _totalLength = totalLength; }

    // GET / SET _sum
    bool hasSum() const { return _hasSum; }

    void setHasSum(bool hasSum) { _hasSum = hasSum; }

    T getSum() const { return _sum; }

    void setSum(T sum) { _sum = sum; }

    // GET / SET _maximum
    bool hasMaximum() const { return _hasMaximum; }

    T getMaximum() const { return _maximum; }

    void setHasMaximum(bool hasMax) { _hasMaximum = hasMax; }

    void setMaximum(T max) { _maximum = max; }

    // GET / SET _minimum
    bool hasMinimum() const { return _hasMinimum; }

    void setHasMinimum(bool hasMin) { _hasMinimum = hasMin; }

    T getMinimum() const { return _minimum; }

    void setMinimum(T min) { _minimum = min; }

    // GET / SET _valueCount
    uint64_t getNumberOfValues() const { return _valueCount; }

    void setNumberOfValues(uint64_t numValues) { _valueCount = numValues; }

    // GET / SET _hasNullValue
    bool hasNull() const { return _hasNull; }

    void setHasNull(bool hasNull) { _hasNull = hasNull; }

    void reset() {
      _hasNull = false;
      _hasMinimum = false;
      _hasMaximum = false;
      _hasSum = false;
      _hasTotalLength = false;
      _totalLength = 0;
      _valueCount = 0;
    }

    void updateMinMax(T value) {
      if (!_hasMinimum) {
        _hasMinimum = _hasMaximum = true;
        _minimum = _maximum = value;
      } else if (compare(value, _minimum)) {
        _minimum = value;
      } else if (compare(_maximum, value)) {
        _maximum = value;
      }
    }

    // sum is not merged here as we need to check overflow
    void merge(const InternalStatisticsImpl& other) {
      _hasNull = _hasNull || other._hasNull;
      _valueCount += other._valueCount;

      if (other._hasMinimum) {
        if (!_hasMinimum) {
          _hasMinimum = _hasMaximum = true;
          _minimum = other._minimum;
          _maximum = other._maximum;
        } else {
          // all template types should support operator<
          if (compare(_maximum, other._maximum)) {
            _maximum = other._maximum;
          }
          if (compare(other._minimum, _minimum)) {
            _minimum = other._minimum;
          }
        }
      }

      _hasTotalLength = _hasTotalLength && other._hasTotalLength;
      _totalLength += other._totalLength;
    }
   };

  typedef InternalStatisticsImpl<char> InternalCharStatistics;
  typedef InternalStatisticsImpl<uint64_t> InternalBooleanStatistics;
  typedef InternalStatisticsImpl<int64_t> InternalIntegerStatistics;
  typedef InternalStatisticsImpl<int32_t> InternalDateStatistics;
  typedef InternalStatisticsImpl<double> InternalDoubleStatistics;
  typedef InternalStatisticsImpl<Decimal> InternalDecimalStatistics;
  typedef InternalStatisticsImpl<std::string> InternalStringStatistics;

/**
 * ColumnStatistics Implementation
 */

  class ColumnStatisticsImpl: public ColumnStatistics {
  private:
    InternalCharStatistics _stats;
  public:
    ColumnStatisticsImpl() { reset(); }
    ColumnStatisticsImpl(const proto::ColumnStatistics& stats);
    virtual ~ColumnStatisticsImpl();

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    void merge(const ColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
    }

    void reset() {
      _stats.reset();
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Column has " << getNumberOfValues() << " values"
             << " and has null value: " << (hasNull() ? "yes" : "no")
             << std::endl;
      return buffer.str();
    }
  };

  class BinaryColumnStatisticsImpl: public BinaryColumnStatistics {
  private:
    InternalCharStatistics _stats;
  public:
    BinaryColumnStatisticsImpl() { reset(); }
    BinaryColumnStatisticsImpl(const proto::ColumnStatistics& stats,
                               const StatContext& statContext);
    virtual ~BinaryColumnStatisticsImpl();

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    bool hasTotalLength() const override {
      return _stats.hasTotalLength();
    }

    uint64_t getTotalLength() const override {
      if(hasTotalLength()){
        return _stats.getTotalLength();
      }else{
        throw ParseError("Total length is not defined.");
      }
    }

    void setTotalLength(uint64_t length) {
      _stats.setHasTotalLength(true);
      _stats.setTotalLength(length);
    }

    void update(size_t length) {
      _stats.setTotalLength(_stats.getTotalLength() + length);
    }

    void merge(const BinaryColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
    }

    void reset() {
      _stats.reset();
      setTotalLength(0);
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::BinaryStatistics* binStats = pbStats.mutable_binarystatistics();
      binStats->set_sum(static_cast<int64_t>(_stats.getTotalLength()));
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Binary" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasTotalLength()){
        buffer << "Total length: " << getTotalLength() << std::endl;
      }else{
        buffer << "Total length: not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class BooleanColumnStatisticsImpl: public BooleanColumnStatistics {
  private:
    InternalBooleanStatistics _stats;
    bool _hasCount;
    uint64_t _trueCount;

  public:
    BooleanColumnStatisticsImpl() { reset(); }
    BooleanColumnStatisticsImpl(const proto::ColumnStatistics& stats, const StatContext& statContext);
    virtual ~BooleanColumnStatisticsImpl();

    bool hasCount() const override {
      return _hasCount;
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
      _hasCount = true;
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    uint64_t getFalseCount() const override {
      if(hasCount()){
        return getNumberOfValues() - _trueCount;
      }else{
        throw ParseError("False count is not defined.");
      }
    }

    uint64_t getTrueCount() const override {
      if(hasCount()){
        return _trueCount;
      }else{
        throw ParseError("True count is not defined.");
      }
    }

    void setTrueCount(uint64_t trueCount) {
      _hasCount = true;
      _trueCount = trueCount;
    }

    void update(bool value, size_t repetitions) {
      if (value) {
        _trueCount += repetitions;
      }
    }

    void merge(const BooleanColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
      _hasCount = _hasCount && other._hasCount;
      _trueCount += other._trueCount;
    }

    void reset() {
      _stats.reset();
      setTrueCount(0);
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::BucketStatistics* bucketStats = pbStats.mutable_bucketstatistics();
      if (_hasCount) {
        bucketStats->add_count(_trueCount);
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Boolean" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasCount()){
        buffer << "(true: " << getTrueCount() << "; false: "
               << getFalseCount() << ")" << std::endl;
      } else {
        buffer << "(true: not defined; false: not defined)" << std::endl;
        buffer << "True and false count are not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class DateColumnStatisticsImpl: public DateColumnStatistics {
  private:
    InternalDateStatistics _stats; 
  public:
    DateColumnStatisticsImpl() { reset(); }
    DateColumnStatisticsImpl(const proto::ColumnStatistics& stats, const StatContext& statContext);
    virtual ~DateColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    int32_t getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    int32_t getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(int32_t minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(int32_t maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    void update(int32_t value) {
      _stats.updateMinMax(value);
    }

    void merge(const DateColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
    }

    void reset() {
      _stats.reset();
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      if (_stats.hasMinimum()) {
        proto::DateStatistics* dateStatistics = pbStats.mutable_datestatistics();
        dateStatistics->set_maximum(_stats.getMaximum());
        dateStatistics->set_minimum(_stats.getMinimum());
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Date" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        buffer << "Minimum: " << getMinimum() << std::endl;
      }else{
        buffer << "Minimum: not defined" << std::endl;
      }

      if(hasMaximum()){
        buffer << "Maximum: " << getMaximum() << std::endl;
      }else{
        buffer << "Maximum: not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class DecimalColumnStatisticsImpl: public DecimalColumnStatistics {
  private:
    InternalDecimalStatistics _stats; 

  public:
    DecimalColumnStatisticsImpl() { reset(); }
    DecimalColumnStatisticsImpl(const proto::ColumnStatistics& stats, const StatContext& statContext);
    virtual ~DecimalColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    bool hasSum() const override {
      return _stats.hasSum();
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    Decimal getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    Decimal getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(Decimal minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(Decimal maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    Decimal getSum() const override {
      if(hasSum()){
        return _stats.getSum();
      }else{
        throw ParseError("Sum is not defined.");
      }
    }

    void setSum(Decimal sum) {
      _stats.setHasSum(true);
      _stats.setSum(sum);
    }

    void update(const Decimal& value) {
      _stats.updateMinMax(value);

      if (_stats.hasSum()) {
        updateSum(value);
      }
    }

    void merge(const DecimalColumnStatisticsImpl& other) {
      _stats.merge(other._stats);

      _stats.setHasSum(_stats.hasSum() && other.hasSum());
      if (_stats.hasSum()) {
        updateSum(other.getSum());
      }
    }

    void reset() {
      _stats.reset();
      setSum(Decimal());
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::DecimalStatistics* decStats = pbStats.mutable_decimalstatistics();
      if (_stats.hasMinimum()) {
        decStats->set_minimum(_stats.getMinimum().toString());
        decStats->set_maximum(_stats.getMaximum().toString());
      }
      if (_stats.hasSum()) {
        decStats->set_sum(_stats.getSum().toString());
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Decimal" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        buffer << "Minimum: " << getMinimum().toString() << std::endl;
      }else{
        buffer << "Minimum: not defined" << std::endl;
      }

      if(hasMaximum()){
        buffer << "Maximum: " << getMaximum().toString() << std::endl;
      }else{
        buffer << "Maximum: not defined" << std::endl;
      }

      if(hasSum()){
        buffer << "Sum: " << getSum().toString() << std::endl;
      }else{
        buffer << "Sum: not defined" << std::endl;
      }

      return buffer.str();
    }

  private:
    void updateSum(Decimal value) {
      if (_stats.hasSum()) {
        bool overflow = false;
        Decimal sum = _stats.getSum();
        if (sum.scale > value.scale) {
          value.value = scaleUpInt128ByPowerOfTen(value.value,
                                                  sum.scale - value.scale,
                                                  overflow);
        } else if (sum.scale < value.scale) {
          sum.value = scaleUpInt128ByPowerOfTen(sum.value,
                                                value.scale - sum.scale,
                                                overflow);
          sum.scale = value.scale;
        }

        if (!overflow) {
          bool wasPositive = sum.value >= 0;
          sum.value += value.value;
          if ((value.value >= 0) == wasPositive) {
            _stats.setHasSum((sum.value >= 0) == wasPositive);
          }
        } else {
          _stats.setHasSum(false);
        }

        if (_stats.hasSum()) {
          _stats.setSum(sum);
        }
      }
    }
  };

  class DoubleColumnStatisticsImpl: public DoubleColumnStatistics {
  private:
    InternalDoubleStatistics _stats;
  public:
    DoubleColumnStatisticsImpl() { reset(); }
    DoubleColumnStatisticsImpl(const proto::ColumnStatistics& stats);
    virtual ~DoubleColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    bool hasSum() const override {
      return _stats.hasSum();
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    double getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    double getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(double minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(double maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    double getSum() const override {
      if(hasSum()){
        return _stats.getSum();
      }else{
        throw ParseError("Sum is not defined.");
      }
    }

    void setSum(double sum) {
      _stats.setHasSum(true);
      _stats.setSum(sum);
    }

    void update(double value) {
      _stats.updateMinMax(value);
      _stats.setSum(_stats.getSum() + value);
    }

    void merge(const DoubleColumnStatisticsImpl& other) {
      _stats.merge(other._stats);

      _stats.setHasSum(_stats.hasSum() && other.hasSum());
      if (_stats.hasSum()) {
        _stats.setSum(_stats.getSum() + other.getSum());
      }
    }

    void reset() {
      _stats.reset();
      setSum(0.0);
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::DoubleStatistics* doubleStats = pbStats.mutable_doublestatistics();
      if (_stats.hasMinimum()) {
        doubleStats->set_minimum(_stats.getMinimum());
        doubleStats->set_maximum(_stats.getMaximum());
      }
      if (_stats.hasSum()) {
        doubleStats->set_sum(_stats.getSum());
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Double" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        buffer << "Minimum: " << getMinimum() << std::endl;
      }else{
        buffer << "Minimum: not defined" << std::endl;
      }

      if(hasMaximum()){
        buffer << "Maximum: " << getMaximum() << std::endl;
      }else{
        buffer << "Maximum: not defined" << std::endl;
      }

      if(hasSum()){
        buffer << "Sum: " << getSum() << std::endl;
      }else{
        buffer << "Sum: not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class IntegerColumnStatisticsImpl: public IntegerColumnStatistics {
  private:
    InternalIntegerStatistics _stats;
  public:
    IntegerColumnStatisticsImpl() { reset(); }
    IntegerColumnStatisticsImpl(const proto::ColumnStatistics& stats);
    virtual ~IntegerColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    bool hasSum() const override {
      return _stats.hasSum();
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    int64_t getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    int64_t getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(int64_t minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(int64_t maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    int64_t getSum() const override {
      if(hasSum()){
        return _stats.getSum();
      }else{
        throw ParseError("Sum is not defined.");
      }
    }

    void setSum(int64_t sum) {
      _stats.setHasSum(true);
      _stats.setSum(sum);
    }

    void update(int64_t value, int repetitions) {
      _stats.updateMinMax(value);

      if (_stats.hasSum()) {
        bool wasPositive = _stats.getSum() >= 0;
        _stats.setSum(value * repetitions + _stats.getSum());
        if ((value >= 0) == wasPositive) {
          _stats.setHasSum((_stats.getSum() >= 0) == wasPositive);
        }
      }
    }

    void merge(const IntegerColumnStatisticsImpl& other) {
      _stats.merge(other._stats);

      // update sum and check overflow
      _stats.setHasSum(_stats.hasSum() && other.hasSum());
      if (_stats.hasSum()) {
        bool wasPositive = _stats.getSum() >= 0;
        _stats.setSum(_stats.getSum() + other.getSum());
        if ((other.getSum() >= 0) == wasPositive) {
          _stats.setHasSum((_stats.getSum() >= 0) == wasPositive);
        }
      }
    }

    void reset() {
      _stats.reset();
      setSum(0);
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::IntegerStatistics* intStats = pbStats.mutable_intstatistics();
      if (_stats.hasMinimum()) {
        intStats->set_minimum(_stats.getMinimum());
        intStats->set_maximum(_stats.getMaximum());
      }
      if (_stats.hasSum()) {
        intStats->set_sum(_stats.getSum());
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: Integer" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        buffer << "Minimum: " << getMinimum() << std::endl;
      }else{
        buffer << "Minimum: not defined" << std::endl;
      }

      if(hasMaximum()){
        buffer << "Maximum: " << getMaximum() << std::endl;
      }else{
        buffer << "Maximum: not defined" << std::endl;
      }

      if(hasSum()){
        buffer << "Sum: " << getSum() << std::endl;
      }else{
        buffer << "Sum: not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class StringColumnStatisticsImpl: public StringColumnStatistics {
  private:
    InternalStringStatistics _stats;
    bool _enableStringComparison;

  public:
    StringColumnStatisticsImpl(bool enableStringComparision) {
      _enableStringComparison = enableStringComparision;
      reset();
    }
    StringColumnStatisticsImpl(const proto::ColumnStatistics& stats, const StatContext& statContext);
    virtual ~StringColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    bool hasTotalLength() const override {
      return _stats.hasTotalLength();
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    std::string getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    std::string getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(std::string minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(std::string maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    uint64_t getTotalLength() const override {
      if(hasTotalLength()){
        return _stats.getTotalLength();
      }else{
        throw ParseError("Total length is not defined.");
      }
    }

    void setTotalLength(uint64_t length) {
      _stats.setHasTotalLength(true);
      _stats.setTotalLength(length);
    }

    void update(const char* value, size_t length) {
      if (_enableStringComparison && value != nullptr) {
        if (!_stats.hasMinimum()) {
          setMinimum(std::string(value, value + length));
          setMaximum(std::string(value, value + length));
        } else {
          // update min
          int minCmp = strncmp(_stats.getMinimum().c_str(),
                               value,
                               std::min(_stats.getMinimum().length(), length));
          if (minCmp > 0 || (minCmp == 0 && length < _stats.getMinimum().length())) {
            setMinimum(std::string(value, value + length));
          }

          // update max
          int maxCmp = strncmp(_stats.getMaximum().c_str(),
                               value,
                               std::min(_stats.getMaximum().length(), length));
          if (maxCmp < 0 || (maxCmp == 0 && length > _stats.getMaximum().length())) {
            setMaximum(std::string(value, value + length));
          }
        }
      }

      _stats.setTotalLength(_stats.getTotalLength() + length);
    }

    void update(std::string value) {
      update(value.c_str(), value.length());
    }

    void merge(const StringColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
    }

    void reset() {
      _stats.reset();
      setTotalLength(0);
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      proto::StringStatistics* strStats = pbStats.mutable_stringstatistics();
      if (_stats.hasMinimum()) {
        strStats->set_minimum(_stats.getMinimum());
        strStats->set_maximum(_stats.getMaximum());
      }
      if (_stats.hasTotalLength()) {
        strStats->set_sum(static_cast<int64_t>(_stats.getTotalLength()));
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      buffer << "Data type: String" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        buffer << "Minimum: " << getMinimum() << std::endl;
      }else{
        buffer << "Minimum is not defined" << std::endl;
      }

      if(hasMaximum()){
        buffer << "Maximum: " << getMaximum() << std::endl;
      }else{
        buffer << "Maximum is not defined" << std::endl;
      }

      if(hasTotalLength()){
        buffer << "Total length: " << getTotalLength() << std::endl;
      }else{
        buffer << "Total length is not defined" << std::endl;
      }
      return buffer.str();
    }
  };

  class TimestampColumnStatisticsImpl: public TimestampColumnStatistics {
  private:
    InternalIntegerStatistics _stats;
    bool _hasLowerBound;
    bool _hasUpperBound;
    int64_t _lowerBound;
    int64_t _upperBound;

  public:
    TimestampColumnStatisticsImpl() { reset(); }
    TimestampColumnStatisticsImpl(const proto::ColumnStatistics& stats,
                                  const StatContext& statContext);
    virtual ~TimestampColumnStatisticsImpl();

    bool hasMinimum() const override {
      return _stats.hasMinimum();
    }

    bool hasMaximum() const override {
      return _stats.hasMaximum();
    }

    uint64_t getNumberOfValues() const override {
      return _stats.getNumberOfValues();
    }

    void setNumberOfValues(uint64_t value) {
      _stats.setNumberOfValues(value);
    }

    void increase(uint64_t count) {
      _stats.setNumberOfValues(_stats.getNumberOfValues() + count);
    }

    bool hasNull() const override {
      return _stats.hasNull();
    }

    void setHasNull(bool hasNull) {
      _stats.setHasNull(hasNull);
    }

    int64_t getMinimum() const override {
      if(hasMinimum()){
        return _stats.getMinimum();
      }else{
        throw ParseError("Minimum is not defined.");
      }
    }

    int64_t getMaximum() const override {
      if(hasMaximum()){
        return _stats.getMaximum();
      }else{
        throw ParseError("Maximum is not defined.");
      }
    }

    void setMinimum(int64_t minimum) {
      _stats.setHasMinimum(true);
      _stats.setMinimum(minimum);
    }

    void setMaximum(int64_t maximum) {
      _stats.setHasMaximum(true);
      _stats.setMaximum(maximum);
    }

    void update(int64_t value) {
      _stats.updateMinMax(value);
    }

    void merge(const TimestampColumnStatisticsImpl& other) {
      _stats.merge(other._stats);
    }

    void reset() {
      _stats.reset();
    }

    void toProtoBuf(proto::ColumnStatistics& pbStats) const {
      pbStats.set_hasnull(_stats.hasNull());
      pbStats.set_numberofvalues(_stats.getNumberOfValues());

      if (_stats.hasMinimum()) {
        proto::TimestampStatistics* tsStats =
          pbStats.mutable_timestampstatistics();
        tsStats->set_minimumutc(_stats.getMinimum());
        tsStats->set_maximumutc(_stats.getMaximum());
      }
    }

    std::string toString() const override {
      std::ostringstream buffer;
      struct tm tmValue;
      char timeBuffer[20];
      time_t secs = 0;

      buffer << "Data type: Timestamp" << std::endl
             << "Values: " << getNumberOfValues() << std::endl
             << "Has null: " << (hasNull() ? "yes" : "no") << std::endl;
      if(hasMinimum()){
        secs = static_cast<time_t>(getMinimum() / 1000);
        gmtime_r(&secs, &tmValue);
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmValue);
        buffer << "Minimum: " << timeBuffer << "." << (getMinimum() % 1000) << std::endl;
      }else{
        buffer << "Minimum is not defined" << std::endl;
      }

      if(hasLowerBound()){
        secs = static_cast<time_t>(getLowerBound() / 1000);
        gmtime_r(&secs, &tmValue);
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmValue);
        buffer << "LowerBound: " << timeBuffer << "." << (getLowerBound() % 1000) << std::endl;
      }else{
        buffer << "LowerBound is not defined" << std::endl;
      }

      if(hasMaximum()){
        secs = static_cast<time_t>(getMaximum()/1000);
        gmtime_r(&secs, &tmValue);
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmValue);
        buffer << "Maximum: " << timeBuffer << "." << (getMaximum() % 1000) << std::endl;
      }else{
        buffer << "Maximum is not defined" << std::endl;
      }

      if(hasUpperBound()){
        secs = static_cast<time_t>(getUpperBound() / 1000);
        gmtime_r(&secs, &tmValue);
        strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", &tmValue);
        buffer << "UpperBound: " << timeBuffer << "." << (getUpperBound() % 1000) << std::endl;
      }else{
        buffer << "UpperBound is not defined" << std::endl;
      }

      return buffer.str();
    }

    bool hasLowerBound() const override {
      return _hasLowerBound;
    }

    bool hasUpperBound() const override {
      return _hasUpperBound;
    }

    int64_t getLowerBound() const override {
      if(hasLowerBound()){
        return _lowerBound;
      }else{
        throw ParseError("LowerBound is not defined.");
      }
    }

    int64_t getUpperBound() const override {
      if(hasUpperBound()){
        return _upperBound;
      }else{
        throw ParseError("UpperBound is not defined.");
      }
    }
  };

  ColumnStatistics* convertColumnStatistics(const proto::ColumnStatistics& s,
                                            const StatContext& statContext);

  class StatisticsImpl: public Statistics {
  private:
    std::list<ColumnStatistics*> colStats;

    // DELIBERATELY NOT IMPLEMENTED
    StatisticsImpl(const StatisticsImpl&);
    StatisticsImpl& operator=(const StatisticsImpl&);

  public:
    StatisticsImpl(const proto::StripeStatistics& stripeStats,
                   const StatContext& statContext);

    StatisticsImpl(const proto::Footer& footer, const StatContext& statContext);

    virtual const ColumnStatistics* getColumnStatistics(uint32_t columnId
                                                        ) const override {
      std::list<ColumnStatistics*>::const_iterator it = colStats.begin();
      std::advance(it, static_cast<int64_t>(columnId));
      return *it;
    }

    virtual ~StatisticsImpl();

    uint32_t getNumberOfColumns() const override {
      return static_cast<uint32_t>(colStats.size());
    }
  };

  class StripeStatisticsImpl: public StripeStatistics {
  private:
    std::unique_ptr<StatisticsImpl> columnStats;
    std::vector<std::vector<std::shared_ptr<const ColumnStatistics> > > rowIndexStats;

    // DELIBERATELY NOT IMPLEMENTED
    StripeStatisticsImpl(const StripeStatisticsImpl&);
    StripeStatisticsImpl& operator=(const StripeStatisticsImpl&);

  public:
    StripeStatisticsImpl(const proto::StripeStatistics& stripeStats,
                   std::vector<std::vector<proto::ColumnStatistics> >& indexStats,
                   const StatContext& statContext);

    virtual const ColumnStatistics* getColumnStatistics(uint32_t columnId
                                                        ) const override {
      return columnStats->getColumnStatistics(columnId);
    }

    uint32_t getNumberOfColumns() const override {
      return columnStats->getNumberOfColumns();
    }

    virtual const ColumnStatistics* getRowIndexStatistics(uint32_t columnId, uint32_t rowIndex
                                                        ) const override {
      // check id indices are valid
      return rowIndexStats[columnId][rowIndex].get();
    }

    virtual ~StripeStatisticsImpl();

    uint32_t getNumberOfRowIndexStats(uint32_t columnId) const override {
      return static_cast<uint32_t>(rowIndexStats[columnId].size());
    }
  };

  /**
   * Create ColumnStatistics for writers
   * @param type of column
   * @param enableStringComparison whether enable string columns comparision
   * @return ColumnStatistics instances
   */
  std::unique_ptr<ColumnStatistics> createColumnStatistics(
    const Type& type, bool enableStringComparison);

}// namespace

#endif