// Copyright Vespa.ai. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.

#include "map_listener.h"
#include <vespa/log/log.h>

LOG_SETUP(".slobrok.server.map_listener");

namespace slobrok {

MapListener::~MapListener() = default;

void MapListener::update(const ServiceMapping &old_mapping,
                         const ServiceMapping &new_mapping)
{
    LOG_ASSERT(old_mapping.name == new_mapping.name);
    remove(old_mapping);
    add(new_mapping);
}

} // namespace slobrok

