// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include <vespa/searchlib/attribute/attribute_operation.h>
#include <vespa/searchlib/attribute/attributefactory.h>
#include <vespa/searchlib/attribute/attribute.h>
#include <vespa/searchlib/common/bitvector.h>
#include <vespa/searchcommon/attribute/config.h>
#include <vespa/vespalib/gtest/gtest.h>

#include <vespa/log/log.h>
LOG_SETUP("attribute_operation_test");

using search::attribute::AttributeOperation;
using search::attribute::BasicType;
using search::AttributeVector;
using search::AttributeFactory;
using search::attribute::CollectionType;
using search::attribute::Config;

TEST(AttributeOperationTest, test_legal_operations_on_integer_attribute) {
    const std::vector<uint32_t> docs;
    for (auto operation : {"++", "--", "+=7", "+= 7", "-=7", "*=8", "/=6", "%=7", "=3", "=-3"}) {
        EXPECT_TRUE(AttributeOperation::create(BasicType::INT64, operation, docs));
    }
}

TEST(AttributeOperationTest, test_illegal_operations_on_integer_attribute) {
    const std::vector<uint32_t> docs;
    for (auto operation : {"", "-", "+", "+=7.1", "=a", "*=8.z", "=", "=.7", "/=0", "%=0"}) {
        EXPECT_FALSE(AttributeOperation::create(BasicType::INT64, operation, docs));
    }
}

TEST(AttributeOperationTest, test_legal_operations_on_float_attribute) {
    const std::vector<uint32_t> docs;
    for (auto operation : {"++", "--", "+=7", "+= 7", "-=7", "*=8", "*=8.7", "*=.7", "/=6", "%=7", "=3", "=-3"}) {
        EXPECT_TRUE(AttributeOperation::create(BasicType::DOUBLE, operation, docs));
    }
}

TEST(AttributeOperationTest, test_illegal_operations_on_float_attribute) {
    const std::vector<uint32_t> docs;
    for (auto operation : {"", "-", "+", "=a", "*=8.z", "=", "/=0", "%=0"}) {
        EXPECT_FALSE(AttributeOperation::create(BasicType::DOUBLE, operation, docs));
    }
}

AttributeVector::SP
createAttribute(BasicType basicType, const std::string &fieldName, bool fastSearch = false, bool immutable = false)
{
    constexpr size_t NUM_DOCS = 20;
    Config cfg(basicType, CollectionType::SINGLE);
    cfg.setMutable(!immutable)
       .setFastSearch(fastSearch);
    auto av = search::AttributeFactory::createAttribute(fieldName, cfg);
    while (NUM_DOCS >= av->getNumDocs()) {
        AttributeVector::DocId checkDocId(0u);
        EXPECT_TRUE(av->addDoc(checkDocId));
        EXPECT_EQ(immutable, av->isUndefined(checkDocId));
    }
    av->commit();
    EXPECT_EQ(immutable, av->isUndefined(NUM_DOCS/2));
    return av;
}

template <typename T, typename A, typename R>
void verify(BasicType type, std::string_view operation, AttributeVector & attr, T initial, T expected, std::vector<uint32_t> docs, R result)
{
    auto & attrT = dynamic_cast<A &>(attr);
    for (uint32_t docid(0); docid < attr.getNumDocs(); docid++) {
        attrT.update(docid, initial);
    }
    attr.commit();
    auto op = AttributeOperation::create(type, operation, std::move(result));
    EXPECT_TRUE(op);
    op->operator()(attr);
    for (uint32_t docid(0); docid < attr.getNumDocs(); docid++) {
        if (docs.empty() || (docid < docs.front())) {
            EXPECT_EQ(initial, attrT.get(docid));
        } else {
            EXPECT_EQ(expected, attrT.get(docid));
            docs.erase(docs.begin());
        }
    }
}

template <typename T, typename R>
void verify2(BasicType typeClaimed, std::string_view operation, AttributeVector & attr, T initial, T expected, std::vector<uint32_t> docs, R result) {
    BasicType::Type type = attr.getBasicType();
    if (type == BasicType::INT64) {
        verify<int64_t, search::IntegerAttributeTemplate<int64_t>, R>(typeClaimed, operation, attr, initial, expected, docs, std::move(result));
    } else if (type == BasicType::INT32) {
        verify<int32_t, search::IntegerAttributeTemplate<int32_t>, R>(typeClaimed, operation, attr, initial, expected, docs, std::move(result));
    } else if (type == BasicType::DOUBLE) {
        verify<double , search::FloatingPointAttributeTemplate<double>, R>(typeClaimed, operation, attr, initial, expected, docs, std::move(result));
    } else if (type == BasicType::FLOAT) {
        verify<float , search::FloatingPointAttributeTemplate<float>, R>(typeClaimed, operation, attr, initial, expected, docs, std::move(result));
    } else {
        ASSERT_TRUE(false);
    }
}

template <typename T>
void verify(BasicType typeClaimed, std::string_view operation, AttributeVector & attr, T initial, T expected) {
    SCOPED_TRACE(operation);
    std::vector<uint32_t> docs = {1,4,7,9,10,17,19};
    {
        verify2<T, std::vector<uint32_t>>(typeClaimed, operation, attr, initial, expected, docs, docs);
    }
    {
        std::vector<AttributeOperation::Hit> hits;
        std::for_each(docs.begin(), docs.end(), [&hits](uint32_t docId) { hits.emplace_back(docId, 0.0); });
        verify2<T, std::vector<AttributeOperation::Hit>>(typeClaimed, operation, attr, initial, expected, docs, hits);
    }
    {
        // Only Array hits
        AttributeOperation::FullResult hits;
        std::for_each(docs.begin(), docs.end(), [&hits](uint32_t docId) {
                                                    hits.second.push_back(search::RankedHit(docId, 0.0));
                                                });
        verify2<T, AttributeOperation::FullResult>(typeClaimed, operation, attr, initial, expected, docs, std::move(hits));
    }
    {
        // Only BitVector
        AttributeOperation::FullResult hits;
        hits.first = search::BitVector::create(docs.back() + 1);
        std::for_each(docs.begin(), docs.end(), [&hits](uint32_t docId) {
            hits.first->setBit(docId);
        });
        verify2<T, AttributeOperation::FullResult>(typeClaimed, operation, attr, initial, expected, docs, std::move(hits));
    }
    {
        // And a nice mix
        AttributeOperation::FullResult hits;
        hits.first = search::BitVector::create(docs.back() + 1);
        std::for_each(docs.begin(), docs.end(), [&hits](uint32_t docId) {
            if ((docId%2) == 0) hits.first->setBit(docId);
        });
        std::for_each(docs.begin(), docs.end(), [&hits](uint32_t docId) {
            if ((docId%2) != 0) hits.second.push_back(search::RankedHit(docId, 0.0));
        });
        verify2<T, AttributeOperation::FullResult>(typeClaimed, operation, attr, initial, expected, docs, std::move(hits));
    }
}

template <typename T>
void verify(std::string_view operation, AttributeVector & attr, T initial, T expected) {
    verify<T>(attr.getBasicType(), operation, attr, initial, expected);
}

TEST(AttributeOperationTest, test_all_integer_operations) {
    auto attr = createAttribute(BasicType::INT64, "ai");
    const std::vector<std::pair<const char *, int64_t>> expectedOperation = {
        {"++", 8}, {"--", 6}, {"+=7", 14}, {"-=9", -2}, {"*=3", 21}, {"/=3", 2}, {"%=3", 1}
    };
    for (auto operation : expectedOperation) {
        verify<int64_t>(operation.first, *attr, 7, operation.second);
    }
}

TEST(AttributeOperationTest, test_all_float_operations) {
    auto attr = createAttribute(BasicType::DOUBLE, "af");
    const std::vector<std::pair<const char *, double>> expectedOperation = {
            {"++", 8}, {"--", 6}, {"+=7.3", 14.3}, {"-=0.9", 6.1}, {"*=3.1", 21.7}, {"/=2", 3.5}, {"%=3", 7}
    };
    for (auto operation : expectedOperation) {
       verify<double>(operation.first, *attr, 7, operation.second);
    }
}

TEST(AttributeOperationTest, test_that_even_slightly_mismatching_type_will_fail_to_update) {
    auto attr = createAttribute(BasicType::INT32, "ai");
    for (auto operation : {"++", "--", "+=7", "-=9", "*=3", "/=3", "%=3"}) {
        verify<int64_t>(BasicType::INT64, operation, *attr, 7, 7);
    }
}

TEST(AttributeOperationTest, test_that_fastsearch_attributes_will_fail_to_update) {
    auto attr = createAttribute(BasicType::INT64, "ai", true);
    for (auto operation : {"++", "--", "+=7", "-=9", "*=3", "/=3", "%=3"}) {
        verify<int64_t>(BasicType::INT64, operation, *attr, 7, 7);
    }
}

TEST(AttributeOperationTest, test_that_immutable_attributes_will_fail_to_update) {
    auto attr = createAttribute(BasicType::INT64, "ai", true, false);
    for (auto operation : {"++", "--", "+=7", "-=9", "*=3", "/=3", "%=3"}) {
        verify<int64_t>(BasicType::INT64, operation, *attr, 7, 7);
    }
}

GTEST_MAIN_RUN_ALL_TESTS()
