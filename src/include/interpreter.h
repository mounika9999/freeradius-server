/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#ifndef _FR_INTERPRETER_H
#define _FR_INTERPRETER_H
/**
 * $Id$
 *
 * @file include/interpreter.h
 * @brief The outside interface to interpreter.
 *
 * @author Alan DeKok <aland@freeradius.org>
 */
#include <freeradius-devel/conf_file.h> /* Need CONF_* definitions */
#include <freeradius-devel/map_proc.h>
#include <freeradius-devel/modpriv.h>
#include <freeradius-devel/rad_assert.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UNLANG_STACK_MAX (64)

/* Actions may be a positive integer (the highest one returned in the group
 * will be returned), or the keyword "return", represented here by
 * MOD_ACTION_RETURN, to cause an immediate return.
 * There's also the keyword "reject", represented here by MOD_ACTION_REJECT
 * to cause an immediate reject. */
#define MOD_ACTION_RETURN  (-1)
#define MOD_ACTION_REJECT  (-2)
#define MOD_PRIORITY_MAX   (64)

/** Types of unlang_t nodes
 *
 * Here are our basic types: unlang_t, unlang_group_t, and unlang_module_call_t. For an
 * explanation of what they are all about, see doc/configurable_failover.rst
 */
typedef enum {
	UNLANG_TYPE_NULL = 0,			//!< Modcallable type not set.
	UNLANG_TYPE_MODULE_CALL = 1,		//!< Module method.
	UNLANG_TYPE_GROUP,			//!< Grouping section.
	UNLANG_TYPE_LOAD_BALANCE,		//!< Load balance section.
	UNLANG_TYPE_REDUNDANT_LOAD_BALANCE,	//!< Redundant load balance section.
	UNLANG_TYPE_PARALLEL,			//!< execute statements in parallel
#ifdef WITH_UNLANG
	UNLANG_TYPE_IF,				//!< Condition.
	UNLANG_TYPE_ELSE,			//!< !Condition.
	UNLANG_TYPE_ELSIF,			//!< !Condition && Condition.
	UNLANG_TYPE_UPDATE,			//!< Update block.
	UNLANG_TYPE_SWITCH,			//!< Switch section.
	UNLANG_TYPE_CASE,			//!< Case section (within a #UNLANG_TYPE_SWITCH).
	UNLANG_TYPE_FOREACH,			//!< Foreach section.
	UNLANG_TYPE_BREAK,			//!< Break statement (within a #UNLANG_TYPE_FOREACH).
	UNLANG_TYPE_RETURN,			//!< Return statement.
	UNLANG_TYPE_MAP,			//!< Mapping section (like #UNLANG_TYPE_UPDATE, but uses
						//!< values from a #map_proc_t call).
#endif
	UNLANG_TYPE_POLICY,			//!< Policy section.
	UNLANG_TYPE_XLAT_INLINE,		//!< xlat statement, inline in "unlang"
	UNLANG_TYPE_MODULE_RESUME,		//!< where to resume processing within a module.
	UNLANG_TYPE_MAX
} unlang_type_t;

/** Returned by #unlang_op_t calls, determine the next action of the interpreter
 *
 * These deal exclusively with control flow.
 */
typedef enum {
	UNLANG_ACTION_CALCULATE_RESULT = 1,	//!< Calculate a new section #rlm_rcode_t value.
	UNLANG_ACTION_CONTINUE,			//!< Execute the next #unlang_t.
	UNLANG_ACTION_PUSHED_CHILD,		//!< #unlang_t pushed a new child onto the stack,
						//!< execute it instead of continuing.
	UNLANG_ACTION_BREAK,			//!< Break out of the current group.
	UNLANG_ACTION_STOP_PROCESSING		//!< Break out of processing the current request (unwind).
} unlang_action_t;

typedef enum {
	UNLANG_GROUP_TYPE_SIMPLE = 0,		//!< Execute each of the children sequentially, until we execute
						//!< all of the children, or one returns #UNLANG_ACTION_BREAK.
	UNLANG_GROUP_TYPE_REDUNDANT,		//!< Execute each of the children until one returns a 'good'
						//!< result i.e. ok, updated, noop, then break out of the group.
	UNLANG_GROUP_TYPE_MAX			//!< Number of group types.
} unlang_group_type_t;

/** A node in a graph of #unlang_op_t (s) that we execute
 *
 * The interpreter acts like a turing machine, with #unlang_t nodes forming the tape
 * and the #unlang_action_t the instructions.
 *
 * This is the parent 'class' for multiple #unlang_t node specialisations.
 * The #unlang_t struct is listed first in the specialisation so that we can cast between
 * parent/child classes without knowledge of the layout of the structures.
 *
 * The specialisations of the nodes describe additional details of the operation to be performed.
 */
typedef struct unlang_t {
	struct unlang_t		*parent;	//!< Previous node.
	struct unlang_t		*next;		//!< Next node (executed on #UNLANG_ACTION_CONTINUE et al).
	char const		*name;		//!< Unknown...
	char const 		*debug_name;	//!< Printed in log messages when the node is executed.
	unlang_type_t		type;		//!< The specialisation of this node.
	int			actions[RLM_MODULE_NUMCODES];	//!< Priorities for the various return codes.
} unlang_t;

/** Generic representation of a grouping
 *
 * Can represent IF statements, maps, update sections etc...
 */
typedef struct {
	unlang_t		self;
	unlang_group_type_t	group_type;
	unlang_t		*children;	//!< Children beneath this group.  The body of an if
						//!< section for example.
	unlang_t		*tail;		//!< of the children list.
	CONF_SECTION		*cs;
	int			num_children;

	vp_map_t		*map;		//!< #UNLANG_TYPE_UPDATE, #UNLANG_TYPE_MAP.
	vp_tmpl_t		*vpt;		//!< #UNLANG_TYPE_SWITCH, #UNLANG_TYPE_MAP.
	fr_cond_t		*cond;		//!< #UNLANG_TYPE_IF, #UNLANG_TYPE_ELSIF.

	map_proc_inst_t		*proc_inst;	//!< Instantiation data for #UNLANG_TYPE_MAP.
} unlang_group_t;

/** A call to a module method
 *
 */
typedef struct {
	unlang_t		self;
	module_instance_t	*module_instance;	//!< Instance of the module we're calling.
	module_method_t		method;
} unlang_module_call_t;

/** Pushed onto the interpreter stack by a yielding module, indicates the resumption point
 *
 * Unlike normal coroutines in other languages, we represent resumption points as states in a state
 * machine made up of function pointers.
 *
 * When a module yields, it specifies the function to call when whatever condition is
 * required for resumption is satisfied, it also specifies the ctx for that function,
 * which represents the internal state of the module at the time of yielding.
 *
 * If you want normal coroutine behaviour... ctx is arbitrary and could include a state enum,
 * in which case the function pointer could be the same as the function that yielded, and something
 * like Duff's device could be used to jump back to the yield point.
 *
 * Yield/resume are left as flexible as possible.  Writing async code this way is difficult enough
 * without being straightjacketed.
 */
typedef struct {
	unlang_module_call_t		module;		//!< Module call that returned #RLM_MODULE_YIELD.
							//!< This field must be first, as it includes an
							//!< #unlang_t field which must be at the start
							//!< of every unlang_* structure.

	module_thread_instance_t 	*thread;	//!< thread-local data for this module.
	fr_unlang_module_resume_t	callback;	//!< Function the yielding module indicated should
							//!< be called when the request could be resumed.

	fr_unlang_action_t		signal_callback;  //!< Function the yielding module indicated should
							//!< be called when the request is poked via an action
							//!< may be removed in future.


	void const			*ctx;		//!< Context data for the callback.  Usually represents
							//!< the module's internal state at the time of yielding.
} unlang_module_resumption_t;

/** A naked xlat
 *
 * @note These are vestigial and may be removed in future.
 */
typedef struct {
	unlang_t		self;
	int			exec;
	char			*xlat_name;
} unlang_xlat_inline_t;

/** A module stack entry
 *
 * Represents a single module call.
 */
typedef struct {
	module_thread_instance_t *thread;	//!< thread-local data for this module
} unlang_stack_entry_modcall_t;

/** State of a foreach loop
 *
 */
typedef struct {
	vp_cursor_t		cursor;		//!< Used to track our place in the list we're iterating over.
	VALUE_PAIR 		*vps;		//!< List containing the attribute(s) we're iterating over.
	VALUE_PAIR		*variable;	//!< Attribute we update the value of.
	int			depth;		//!< Level of nesting of this foreach loop.
#ifndef NDEBUG
	int			indent;		//!< for catching indentation issues
#endif
} unlang_stack_entry_foreach_t;

/** State of a redundant operation
 *
 */
typedef struct {
	unlang_t 		*child;
	unlang_t		*found;
} unlang_stack_entry_redundant_t;

typedef union {
	unlang_stack_entry_modcall_t	modcall;	//!< State for a modcall.
	unlang_stack_entry_foreach_t	foreach;	//!< Foreach iterator state.
	unlang_stack_entry_redundant_t	redundant;	//!< Redundant section state.
} unlang_stack_entry_t;

/** Our interpreter stack, as distinct from the C stack
 *
 * We don't call the modules recursively.  Instead we iterate over a list of #unlang_t and
 * and manage the call stack ourselves.
 *
 * After looking at various green thread implementations, it was decided that using the existing
 * unlang interpreter stack was the best way to perform async I/O.
 *
 * Each request as an unlang interpreter stack associated with it, which represents its progress
 * through the server.  Because the interpreter stack is distinct from the C stack, we can have
 * a single system thread with many thousands of pending requests
 */
typedef struct {
	unlang_t		*instruction;			//!< The unlang node we're evaluating.
	rlm_rcode_t		result;
	int			priority;
	unlang_type_t		unwind;				//!< Unwind to this one if it exists.
	bool			do_next_sibling : 1;
	bool			was_if : 1;
	bool			if_taken : 1;
	bool			resume : 1;
	bool			top_frame : 1;

	/** Stack frame specialisations
	 *
	 * These store extra (mutable) state data, for the immutable (#unlang_t)
	 * instruction.  Instructions can't be used to store data because they
	 * might be shared between multiple threads.
	 *
	 * Which stack_entry specialisation to use is determined by the
	 * instruction->type.
	 */
	union {
		unlang_stack_entry_modcall_t	modcall;	//!< State for a modcall.
		unlang_stack_entry_foreach_t	foreach;	//!< Foreach iterator state.
		unlang_stack_entry_redundant_t	redundant;	//!< Redundant section state.
	};
} unlang_stack_frame_t;

/** An unlang stack associated with a request
 *
 */
typedef struct {
	int			depth;		//!< Current depth we're executing at.
	unlang_stack_frame_t	frame[UNLANG_STACK_MAX];	//!< The stack...
} unlang_stack_t;

typedef unlang_action_t (*unlang_op_func_t)(REQUEST *request, unlang_stack_t *stack,
					    rlm_rcode_t *presult, int *priority);

/** An unlang operation
 *
 * These are like the opcodes in other interpreters.  Each operation, when executed
 * will return an #unlang_action_t, which determines what the interpreter does next.
 */
typedef struct {
	char const		*name;		//!< Name of the operation.
	unlang_op_func_t	func;		//!< Function pointer, that we call to perform the operation.
	bool			debug_braces;	//!< Whether the operation needs to print braces in debug mode
} unlang_op_t;

extern unlang_op_t unlang_ops[];

#define MOD_NUM_TYPES (UNLANG_TYPE_XLAT + 1)

extern char const *const comp2str[];

/** @name Conversion functions for converting #unlang_t to its specialisations
 *
 * Simple conversions: #unlang_module_call_t and #unlang_group_t are subclasses of #unlang_t,
 * so we often want to go back and forth between them.
 *
 * @{
 */
static inline unlang_module_call_t *unlang_generic_to_module_call(unlang_t *p)
{
	rad_assert(p->type == UNLANG_TYPE_MODULE_CALL);
	return talloc_get_type_abort(p, unlang_module_call_t);
}

static inline unlang_group_t *unlang_generic_to_group(unlang_t *p)
{
	rad_assert((p->type > UNLANG_TYPE_MODULE_CALL) && (p->type <= UNLANG_TYPE_POLICY));

	return (unlang_group_t *)p;
}

static inline unlang_t *unlang_module_call_to_generic(unlang_module_call_t *p)
{
	return (unlang_t *)p;
}

static inline unlang_t *unlang_group_to_generic(unlang_group_t *p)
{
	return (unlang_t *)p;
}

static inline unlang_xlat_inline_t *unlang_generic_to_xlat_inline(unlang_t *p)
{
	rad_assert(p->type == UNLANG_TYPE_XLAT_INLINE);
	return talloc_get_type_abort(p, unlang_xlat_inline_t);
}

static inline unlang_t *unlang_xlat_inline_to_generic(unlang_xlat_inline_t *p)
{
	return (unlang_t *)p;
}

static inline unlang_module_resumption_t *unlang_generic_to_module_resumption(unlang_t *p)
{
	rad_assert(p->type == UNLANG_TYPE_MODULE_RESUME);
	return talloc_get_type_abort(p, unlang_module_resumption_t);
}

static inline unlang_t *unlang_module_resumption_to_generic(unlang_module_resumption_t *p)
{
	return (unlang_t *)p;
}
/* @} **/

#ifdef __cplusplus
}
#endif

#endif
