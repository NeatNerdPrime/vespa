// Copyright 2018 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
package com.yahoo.vespa.flags.json;

import com.yahoo.vespa.flags.FetchVector;
import com.yahoo.vespa.flags.JsonNodeRawFlag;
import com.yahoo.vespa.flags.RawFlag;
import com.yahoo.vespa.flags.json.wire.WireRule;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * @author hakonhall
 */
public class Rule {
    private final List<Condition> andConditions;
    private final Optional<RawFlag> valueToApply;

    public Rule(Optional<RawFlag> valueToApply, Condition... andConditions) {
        this(valueToApply, Arrays.asList(andConditions));
    }

    public Rule(Optional<RawFlag> valueToApply, List<Condition> andConditions) {
        this.andConditions = andConditions;
        this.valueToApply = valueToApply;
    }

    public boolean match(FetchVector fetchVector) {
        return andConditions.stream().allMatch(condition -> condition.test(fetchVector));
    }

    public Optional<RawFlag> getValueToApply() {
        return valueToApply;
    }

    public WireRule toWire() {
        WireRule wireRule = new WireRule();

        if (!andConditions.isEmpty()) {
            wireRule.andConditions = andConditions.stream().map(Condition::toWire).collect(Collectors.toList());
        }

        wireRule.value = valueToApply.map(RawFlag::asJsonNode).orElse(null);

        return wireRule;
    }

    public static Rule fromWire(WireRule wireRule) {
        List<Condition> conditions = wireRule.andConditions == null ?
                Collections.emptyList() :
                wireRule.andConditions.stream().map(Condition::fromWire).collect(Collectors.toList());
        Optional<RawFlag> value = wireRule.value == null ? Optional.empty() : Optional.of(JsonNodeRawFlag.fromJsonNode(wireRule.value));
        return new Rule(value, conditions);
    }
}
