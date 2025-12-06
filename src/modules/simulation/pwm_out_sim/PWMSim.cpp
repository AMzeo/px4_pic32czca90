/****************************************************************************
 *
 *   Copyright (c) 2012-2022 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "PWMSim.hpp"

#include <mathlib/mathlib.h>
#include <px4_platform_common/getopt.h>

#include <uORB/Subscription.hpp>
#include <uORB/topics/parameter_update.h>

#include <px4_platform_common/sem.hpp>
#include <malloc.h>
#include <syslog.h>

#if SAMV7_PWMSIM_TEST_MODE == 1
/*******************************************************************************
 * TEST MODE 1: Minimal with just ScheduledWorkItem (WORKS)
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::hp_default)
{
	PX4_INFO("PWMSim MODE1: constructor entry");
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");
	PX4_INFO("PWMSim MODE1: constructor done");
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
}

void PWMSim::Run()
{
	if (_run_count < 5) {
		PX4_INFO("PWMSim MODE1: Run() count=%d", _run_count);
	}
	_run_count++;

	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	perf_begin(_cycle_perf);

	actuator_outputs_s outputs{};
	outputs.timestamp = hrt_absolute_time();
	outputs.noutputs = 4;
	for (int i = 0; i < 4; i++) {
		outputs.output[i] = 0.0f;
	}
	_actuator_outputs_sim_pub.publish(outputs);

	perf_end(_cycle_perf);
	ScheduleDelayed(20_ms);
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	PX4_INFO("PWMSim MODE1: task_spawn entry");

	bool hil_mode = false;
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;
		default:
			return print_usage("unrecognized flag");
		}
	}

	PX4_INFO("PWMSim MODE1: allocating instance");
	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	PX4_INFO("PWMSim MODE1: storing instance");
	_object.store(instance);
	_task_id = task_id_is_work_queue;

	PX4_INFO("PWMSim MODE1: scheduling");
	instance->ScheduleNow();

	PX4_INFO("PWMSim MODE1: task_spawn done");
	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	PX4_INFO("PWMSim MODE1 - run_count=%d", _run_count);
	perf_print_counter(_cycle_perf);
	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(R"DESCR_STR(
### Description
MODE1: Minimal pwm_out_sim (ScheduledWorkItem only).
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#elif SAMV7_PWMSIM_TEST_MODE == 2
/*******************************************************************************
 * TEST MODE 2: OutputModuleInterface WITHOUT MixingOutput (WORKS)
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	OutputModuleInterface(MODULE_NAME, px4::wq_configurations::hp_default)
{
	PX4_INFO("PWMSim MODE2: constructor entry (OutputModuleInterface, no MixingOutput)");
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");
	PX4_INFO("PWMSim MODE2: constructor done");
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
}

bool PWMSim::updateOutputs(uint16_t outputs[MAX_ACTUATORS], unsigned num_outputs,
			   unsigned num_control_groups_updated)
{
	return false;
}

void PWMSim::Run()
{
	if (_run_count < 5) {
		PX4_INFO("PWMSim MODE2: Run() count=%d", _run_count);
	}
	_run_count++;

	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	perf_begin(_cycle_perf);

	actuator_outputs_s outputs{};
	outputs.timestamp = hrt_absolute_time();
	outputs.noutputs = 4;
	for (int i = 0; i < 4; i++) {
		outputs.output[i] = 0.0f;
	}
	_actuator_outputs_sim_pub.publish(outputs);

	perf_end(_cycle_perf);
	ScheduleDelayed(20_ms);
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	PX4_INFO("PWMSim MODE2: task_spawn entry");

	bool hil_mode = false;
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;
		default:
			return print_usage("unrecognized flag");
		}
	}

	PX4_INFO("PWMSim MODE2: allocating instance");
	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	PX4_INFO("PWMSim MODE2: storing instance");
	_object.store(instance);
	_task_id = task_id_is_work_queue;

	PX4_INFO("PWMSim MODE2: scheduling");
	instance->ScheduleNow();

	PX4_INFO("PWMSim MODE2: task_spawn done");
	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	PX4_INFO("PWMSim MODE2 (OutputModuleInterface, no MixingOutput) - run_count=%d", _run_count);
	perf_print_counter(_cycle_perf);
	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(R"DESCR_STR(
### Description
MODE2: OutputModuleInterface without MixingOutput.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#elif SAMV7_PWMSIM_TEST_MODE == 3
/*******************************************************************************
 * TEST MODE 3: MixingOutput with MINIMAL init (no setAll* calls) - WORKS
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	OutputModuleInterface(MODULE_NAME, px4::wq_configurations::hp_default),
	_mixing_output(PARAM_PREFIX, MAX_ACTUATORS, *this, MixingOutput::SchedulingPolicy::Auto, false, false)
{
	PX4_INFO("PWMSim MODE3: constructor entry (MixingOutput, minimal init)");
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");
	PX4_INFO("PWMSim MODE3: constructor done");
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
}

bool PWMSim::updateOutputs(uint16_t outputs[MAX_ACTUATORS], unsigned num_outputs,
			   unsigned num_control_groups_updated)
{
	return false;
}

void PWMSim::Run()
{
	if (_run_count < 5) {
		PX4_INFO("PWMSim MODE3: Run() count=%d", _run_count);
	}
	_run_count++;

	if (should_exit()) {
		ScheduleClear();
		_mixing_output.unregister();
		exit_and_cleanup();
		return;
	}

	perf_begin(_cycle_perf);

	actuator_outputs_s outputs{};
	outputs.timestamp = hrt_absolute_time();
	outputs.noutputs = 4;
	for (int i = 0; i < 4; i++) {
		outputs.output[i] = 0.0f;
	}
	_actuator_outputs_sim_pub.publish(outputs);

	perf_end(_cycle_perf);
	ScheduleDelayed(20_ms);
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	PX4_INFO("PWMSim MODE3: task_spawn entry");

	bool hil_mode = false;
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;
		default:
			return print_usage("unrecognized flag");
		}
	}

	PX4_INFO("PWMSim MODE3: allocating instance");
	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	PX4_INFO("PWMSim MODE3: storing instance");
	_object.store(instance);
	_task_id = task_id_is_work_queue;

	PX4_INFO("PWMSim MODE3: scheduling");
	instance->ScheduleNow();

	PX4_INFO("PWMSim MODE3: task_spawn done");
	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	PX4_INFO("PWMSim MODE3 (MixingOutput, minimal init) - run_count=%d", _run_count);
	perf_print_counter(_cycle_perf);
	_mixing_output.printStatus();
	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(R"DESCR_STR(
### Description
MODE3: MixingOutput with minimal init.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#elif SAMV7_PWMSIM_TEST_MODE == 4
/*******************************************************************************
 * TEST MODE 4: MixingOutput + setAll* calls, no update()
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	OutputModuleInterface(MODULE_NAME, px4::wq_configurations::hp_default),
	_mixing_output(PARAM_PREFIX, MAX_ACTUATORS, *this, MixingOutput::SchedulingPolicy::Auto, false, false)
{
	PX4_INFO("PWMSim MODE4: constructor entry");
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");

	PX4_INFO("PWMSim MODE4: calling setAllDisarmedValues");
	_mixing_output.setAllDisarmedValues(PWM_SIM_DISARMED_MAGIC);

	PX4_INFO("PWMSim MODE4: calling setAllFailsafeValues");
	_mixing_output.setAllFailsafeValues(PWM_SIM_FAILSAFE_MAGIC);

	PX4_INFO("PWMSim MODE4: calling setAllMinValues");
	_mixing_output.setAllMinValues(PWM_SIM_PWM_MIN_MAGIC);

	PX4_INFO("PWMSim MODE4: calling setAllMaxValues");
	_mixing_output.setAllMaxValues(PWM_SIM_PWM_MAX_MAGIC);

	PX4_INFO("PWMSim MODE4: calling setIgnoreLockdown");
	_mixing_output.setIgnoreLockdown(hil_mode_enabled);

	PX4_INFO("PWMSim MODE4: constructor done");
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
}

bool PWMSim::updateOutputs(uint16_t outputs[MAX_ACTUATORS], unsigned num_outputs,
			   unsigned num_control_groups_updated)
{
	return false;
}

void PWMSim::Run()
{
	if (_run_count < 5) {
		PX4_INFO("PWMSim MODE4: Run() count=%d", _run_count);
	}
	_run_count++;

	if (should_exit()) {
		ScheduleClear();
		_mixing_output.unregister();
		exit_and_cleanup();
		return;
	}

	perf_begin(_cycle_perf);

	/* Don't call _mixing_output.update() yet */
	actuator_outputs_s outputs{};
	outputs.timestamp = hrt_absolute_time();
	outputs.noutputs = 4;
	for (int i = 0; i < 4; i++) {
		outputs.output[i] = 0.0f;
	}
	_actuator_outputs_sim_pub.publish(outputs);

	perf_end(_cycle_perf);
	ScheduleDelayed(20_ms);
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	PX4_INFO("PWMSim MODE4: task_spawn entry");

	bool hil_mode = false;
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;
		default:
			return print_usage("unrecognized flag");
		}
	}

	PX4_INFO("PWMSim MODE4: allocating instance");
	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	PX4_INFO("PWMSim MODE4: storing instance");
	_object.store(instance);
	_task_id = task_id_is_work_queue;

	PX4_INFO("PWMSim MODE4: scheduling");
	instance->ScheduleNow();

	PX4_INFO("PWMSim MODE4: task_spawn done");
	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	PX4_INFO("PWMSim MODE4 - run_count=%d", _run_count);
	perf_print_counter(_cycle_perf);
	_mixing_output.printStatus();
	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(R"DESCR_STR(
### Description
MODE4: MixingOutput + setAll* calls, no update().
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#elif SAMV7_PWMSIM_TEST_MODE == 5
/*******************************************************************************
 * TEST MODE 5: MixingOutput + setAll* + update() call
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	OutputModuleInterface(MODULE_NAME, px4::wq_configurations::hp_default),
	_mixing_output(PARAM_PREFIX, MAX_ACTUATORS, *this, MixingOutput::SchedulingPolicy::Auto, false, false)
{
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");

	_mixing_output.setAllDisarmedValues(PWM_SIM_DISARMED_MAGIC);
	_mixing_output.setAllFailsafeValues(PWM_SIM_FAILSAFE_MAGIC);
	_mixing_output.setAllMinValues(PWM_SIM_PWM_MIN_MAGIC);
	_mixing_output.setAllMaxValues(PWM_SIM_PWM_MAX_MAGIC);
	_mixing_output.setIgnoreLockdown(hil_mode_enabled);
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
}

bool PWMSim::updateOutputs(uint16_t outputs[MAX_ACTUATORS], unsigned num_outputs,
			   unsigned num_control_groups_updated)
{
	// Only publish once we receive actuator_controls (important for lock-step to work correctly)
	if (num_control_groups_updated > 0) {
		actuator_outputs_s actuator_outputs{};
		actuator_outputs.noutputs = num_outputs;

		const uint32_t reversible_outputs = _mixing_output.reversibleOutputs();

		for (int i = 0; i < (int)num_outputs; i++) {
			if (outputs[i] != PWM_SIM_DISARMED_MAGIC) {

				OutputFunction function = _mixing_output.outputFunction(i);
				bool is_reversible = reversible_outputs & (1u << i);
				float output = outputs[i];

				if (((int)function >= (int)OutputFunction::Motor1 && (int)function <= (int)OutputFunction::MotorMax)
				    && !is_reversible) {
					// Scale non-reversible motors to [0, 1]
					actuator_outputs.output[i] = (output - PWM_SIM_PWM_MIN_MAGIC) / (PWM_SIM_PWM_MAX_MAGIC - PWM_SIM_PWM_MIN_MAGIC);

				} else {
					// Scale everything else to [-1, 1]
					const float pwm_center = (PWM_SIM_PWM_MAX_MAGIC + PWM_SIM_PWM_MIN_MAGIC) / 2.f;
					const float pwm_delta = (PWM_SIM_PWM_MAX_MAGIC - PWM_SIM_PWM_MIN_MAGIC) / 2.f;
					actuator_outputs.output[i] = (output - pwm_center) / pwm_delta;
				}
			}
		}

		actuator_outputs.timestamp = hrt_absolute_time();
		_actuator_outputs_sim_pub.publish(actuator_outputs);
		return true;
	}

	return false;
}

void PWMSim::Run()
{
	if (should_exit()) {
		ScheduleClear();
		_mixing_output.unregister();
		exit_and_cleanup();
		return;
	}

	perf_begin(_cycle_perf);

	_mixing_output.update();

	// SAMV7: Skip updateSubscriptions entirely - even with allow_wq_switch=false
	// it causes a crash. Root cause in updateSubscriptions() needs investigation.
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
	_mixing_output.updateSubscriptions(true);
#endif

	perf_end(_cycle_perf);
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	bool hil_mode = false;
	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;
		default:
			return print_usage("unrecognized flag");
		}
	}

	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	_object.store(instance);
	_task_id = task_id_is_work_queue;
	instance->ScheduleNow();

	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	perf_print_counter(_cycle_perf);
	_mixing_output.printStatus();
	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(R"DESCR_STR(
### Description
Driver for simulated PWM outputs (HITL/SITL).

Takes actuator_control uORB messages, mixes them and outputs
the result to actuator_outputs_sim for the simulator.
)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#else
/*******************************************************************************
 * FULL VERSION - With MixingOutput (MODE 0 with debug)
 ******************************************************************************/

PWMSim::PWMSim(bool hil_mode_enabled) :
	OutputModuleInterface(MODULE_NAME, px4::wq_configurations::hp_default),
	_mixing_output(PARAM_PREFIX, MAX_ACTUATORS, *this, MixingOutput::SchedulingPolicy::Auto, false, false),
	_parameter_update_sub(ORB_ID(parameter_update), 1_s)
{
	PX4_INFO("PWMSim MODE0: constructor entry");

	/* Explicit perf counter allocation for SAMV7 compatibility */
	_cycle_perf = perf_alloc(PC_ELAPSED, MODULE_NAME": cycle");
	_interval_perf = perf_alloc(PC_INTERVAL, MODULE_NAME": interval");

	PX4_INFO("PWMSim MODE0: calling setAll*");
	_mixing_output.setAllDisarmedValues(PWM_SIM_DISARMED_MAGIC);
	_mixing_output.setAllFailsafeValues(PWM_SIM_FAILSAFE_MAGIC);
	_mixing_output.setAllMinValues(PWM_SIM_PWM_MIN_MAGIC);
	_mixing_output.setAllMaxValues(PWM_SIM_PWM_MAX_MAGIC);
	_mixing_output.setIgnoreLockdown(hil_mode_enabled);

	PX4_INFO("PWMSim MODE0: constructor done");
}

PWMSim::~PWMSim()
{
	perf_free(_cycle_perf);
	perf_free(_interval_perf);
}

static int _mode0_updateOutputs_count = 0;

bool PWMSim::updateOutputs(uint16_t outputs[MAX_ACTUATORS], unsigned num_outputs,
			   unsigned num_control_groups_updated)
{
	if (_mode0_updateOutputs_count < 3) {
		PX4_INFO("MODE0: updateOutputs num=%u groups=%u", num_outputs, num_control_groups_updated);
	}
	_mode0_updateOutputs_count++;

	// Only publish once we receive actuator_controls (important for lock-step to work correctly)
	if (num_control_groups_updated > 0) {
		actuator_outputs_s actuator_outputs{};
		actuator_outputs.noutputs = num_outputs;

		const uint32_t reversible_outputs = _mixing_output.reversibleOutputs();

		for (int i = 0; i < (int)num_outputs; i++) {
			if (outputs[i] != PWM_SIM_DISARMED_MAGIC) {

				OutputFunction function = _mixing_output.outputFunction(i);
				bool is_reversible = reversible_outputs & (1u << i);
				float output = outputs[i];

				if (((int)function >= (int)OutputFunction::Motor1 && (int)function <= (int)OutputFunction::MotorMax)
				    && !is_reversible) {
					// Scale non-reversible motors to [0, 1]
					actuator_outputs.output[i] = (output - PWM_SIM_PWM_MIN_MAGIC) / (PWM_SIM_PWM_MAX_MAGIC - PWM_SIM_PWM_MIN_MAGIC);

				} else {
					// Scale everything else to [-1, 1]
					const float pwm_center = (PWM_SIM_PWM_MAX_MAGIC + PWM_SIM_PWM_MIN_MAGIC) / 2.f;
					const float pwm_delta = (PWM_SIM_PWM_MAX_MAGIC - PWM_SIM_PWM_MIN_MAGIC) / 2.f;
					actuator_outputs.output[i] = (output - pwm_center) / pwm_delta;
				}
			}
		}

		actuator_outputs.timestamp = hrt_absolute_time();
		_actuator_outputs_sim_pub.publish(actuator_outputs);
		return true;
	}

	return false;
}

void PWMSim::Run()
{
	static bool first_run = true;
	if (first_run) {
		PX4_INFO("pwm_out_sim: Run() first call");
		first_run = false;
	}

	if (should_exit()) {
		ScheduleClear();
		_mixing_output.unregister();

		exit_and_cleanup();
		return;
	}

	_mixing_output.update();

	// check for parameter updates
	if (_parameter_update_sub.updated()) {
		parameter_update_s pupdate;
		_parameter_update_sub.copy(&pupdate);
		updateParams();
	}

	// SAMV7: Skip updateSubscriptions entirely - causes crash even with false.
#if !defined(CONFIG_ARCH_CHIP_SAMV7)
	_mixing_output.updateSubscriptions(true);
#endif
}

int PWMSim::task_spawn(int argc, char *argv[])
{
	bool hil_mode = false;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	while ((ch = px4_getopt(argc, argv, "m:", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'm':
			hil_mode = strcmp(myoptarg, "hil") == 0;
			break;

		default:
			return print_usage("unrecognized flag");
		}
	}

	/* Minimal test - just try to allocate */
	PWMSim *instance = new PWMSim(hil_mode);

	if (!instance) {
		PX4_ERR("alloc failed");
		return -1;
	}

	_object.store(instance);
	_task_id = task_id_is_work_queue;
	instance->ScheduleNow();
	return 0;
}

int PWMSim::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int PWMSim::print_status()
{
	perf_print_counter(_cycle_perf);
	perf_print_counter(_interval_perf);
	_mixing_output.printStatus();

	return 0;
}

int PWMSim::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Driver for simulated PWM outputs.

Its only function is to take `actuator_control` uORB messages,
mix them with any loaded mixer and output the result to the
`actuator_output` uORB topic.

It is used in SITL and HITL.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("pwm_out_sim", "driver");
	PRINT_MODULE_USAGE_COMMAND_DESCR("start", "Start the module");
	PRINT_MODULE_USAGE_PARAM_STRING('m', "sim", "hil|sim", "Mode", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

#endif /* SAMV7_PWMSIM_TEST_MODE */

extern "C" __EXPORT int pwm_out_sim_main(int argc, char *argv[])
{
	return PWMSim::main(argc, argv);
}
