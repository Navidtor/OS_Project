/**
 * ALFS - JSON Handler Interface
 */

#ifndef JSON_HANDLER_H
#define JSON_HANDLER_H

#include "alfs.h"

/**
 * Parse a TimeFrame from JSON string
 * @param json_str JSON string
 * @return TimeFrame structure or NULL on error
 */
TimeFrame *json_parse_timeframe(const char *json_str);

/**
 * Free a TimeFrame structure
 * @param tf TimeFrame to free
 */
void json_free_timeframe(TimeFrame *tf);

/**
 * Serialize a SchedulerTick to JSON string
 * @param tick SchedulerTick to serialize
 * @param include_meta Whether to include metadata
 * @return Dynamically allocated JSON string (caller must free), NULL on error
 */
char *json_serialize_tick(const SchedulerTick *tick, bool include_meta);

/**
 * Parse event action string to enum
 * @param action_str Action string
 * @return EventAction enum value, EVENT_INVALID if unknown
 */
EventAction json_parse_action(const char *action_str);

/**
 * Get action string from enum
 * @param action EventAction enum
 * @return Action string (static, do not free)
 */
const char *json_action_to_string(EventAction action);

#endif /* JSON_HANDLER_H */
