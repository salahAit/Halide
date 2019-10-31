#ifndef HALIDE_INTERNAL_AUTO_SCHEDULE_H
#define HALIDE_INTERNAL_AUTO_SCHEDULE_H

/** \file
 *
 * Defines the method that does automatic scheduling of Funcs within a pipeline.
 */

#include "Errors.h"

#include <vector>
#include <map>
#include <string>

namespace Halide {

namespace Internal {


/** If the cost of computing a Func is about the same as calling the Func,
 * inline the Func. Return true of any of the Funcs is inlined. */
bool inline_all_trivial_functions(const std::vector<Function> &outputs,
                                  const std::vector<std::string> &order,
                                  const std::map<std::string, Function> &env);

/** Determine if a Func (order[index]) is only consumed by another single Func
 * in element-wise manner. If it is, return the name of the consumer Func;
 * otherwise, return an empty string. */
std::string is_func_called_element_wise(const std::vector<std::string> &order, size_t index,
                                        const std::map<std::string, Function> &env);

/** Inline a Func if its values are only consumed by another single Func in
 * element-wise manner. */
bool inline_all_element_wise_functions(const std::vector<Function> &outputs,
                                       const std::vector<std::string> &order,
                                       const std::map<std::string, Function> &env);

/** Generate schedules for Funcs within a pipeline. The Funcs should not already
 * have specializations or schedules as the current auto-scheduler does not take
 * into account user-defined schedules or specializations. This applies the
 * schedules and returns a string representation of the schedules. The target
 * architecture is specified by 'target'. */
std::string generate_schedules(const std::vector<Function> &outputs,
                               const Target &target,
                               const MachineParams &arch_params);

}  // namespace Internal
}  // namespace Halide

#endif