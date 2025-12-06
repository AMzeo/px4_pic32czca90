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

#pragma once

#include <drivers/device/device.h>
#include <drivers/drv_hrt.h>
#include <drivers/drv_pwm_output.h>
#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/module.h>
#include <px4_platform_common/tasks.h>
#include <px4_platform_common/time.h>
#include <px4_platform_common/px4_work_queue/ScheduledWorkItem.hpp>
#include <uORB/topics/parameter_update.h>
#include <uORB/topics/actuator_outputs.h>
#include <uORB/Subscription.hpp>
#include <uORB/Publication.hpp>

/* SAMV7 HITL: Now works with full mode after runtime init fix.
 * See: boards/microchip/samv71-xult-clickboards/UPDATESUBSCRIPTIONS_DEBUG.md
 */
#define SAMV7_PWMSIM_TEST_MODE 0

#if SAMV7_PWMSIM_TEST_MODE == 0 || SAMV7_PWMSIM_TEST_MODE >= 2
#include <lib/mixer_module/mixer_module.hpp>
#endif

#if defined(CONFIG_ARCH_BOARD_PX4_SITL)
#define PARAM_PREFIX "PWM_MAIN"
#else
#define PARAM_PREFIX "HIL_ACT"
#endif

using namespace time_literals;

#if SAMV7_PWMSIM_TEST_MODE == 1
/*******************************************************************************
 * TEST MODE 1: Minimal with just ScheduledWorkItem (WORKS)
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public px4::ScheduledWorkItem
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

private:
	void Run() override;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};
	perf_counter_t	_cycle_perf{nullptr};
	int _run_count{0};
};

#elif SAMV7_PWMSIM_TEST_MODE == 2
/*******************************************************************************
 * TEST MODE 2: OutputModuleInterface WITHOUT MixingOutput (WORKS)
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public OutputModuleInterface
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

	bool updateOutputs(uint16_t outputs[MAX_ACTUATORS],
			   unsigned num_outputs, unsigned num_control_groups_updated) override;

private:
	void Run() override;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};
	perf_counter_t	_cycle_perf{nullptr};
	int _run_count{0};
};

#elif SAMV7_PWMSIM_TEST_MODE == 3
/*******************************************************************************
 * TEST MODE 3: MixingOutput with MINIMAL init (no setAll* calls) - WORKS
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public OutputModuleInterface
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

	bool updateOutputs(uint16_t outputs[MAX_ACTUATORS],
			   unsigned num_outputs, unsigned num_control_groups_updated) override;

private:
	void Run() override;

	MixingOutput _mixing_output;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};
	perf_counter_t	_cycle_perf{nullptr};
	int _run_count{0};
};

#elif SAMV7_PWMSIM_TEST_MODE == 4
/*******************************************************************************
 * TEST MODE 4: MixingOutput + setAll* calls, no update() (WORKS)
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public OutputModuleInterface
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

	bool updateOutputs(uint16_t outputs[MAX_ACTUATORS],
			   unsigned num_outputs, unsigned num_control_groups_updated) override;

private:
	void Run() override;

	static constexpr uint16_t PWM_SIM_DISARMED_MAGIC = 900;
	static constexpr uint16_t PWM_SIM_FAILSAFE_MAGIC = 600;
	static constexpr uint16_t PWM_SIM_PWM_MIN_MAGIC = 1000;
	static constexpr uint16_t PWM_SIM_PWM_MAX_MAGIC = 2000;

	MixingOutput _mixing_output;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};
	perf_counter_t	_cycle_perf{nullptr};
	int _run_count{0};
};

#elif SAMV7_PWMSIM_TEST_MODE == 5
/*******************************************************************************
 * SAMV7 HITL version - verified working
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public OutputModuleInterface
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

	bool updateOutputs(uint16_t outputs[MAX_ACTUATORS],
			   unsigned num_outputs, unsigned num_control_groups_updated) override;

private:
	void Run() override;

	static constexpr uint16_t PWM_SIM_DISARMED_MAGIC = 900;
	static constexpr uint16_t PWM_SIM_FAILSAFE_MAGIC = 600;
	static constexpr uint16_t PWM_SIM_PWM_MIN_MAGIC = 1000;
	static constexpr uint16_t PWM_SIM_PWM_MAX_MAGIC = 2000;

	MixingOutput _mixing_output;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};
	perf_counter_t	_cycle_perf{nullptr};
};

#else
/*******************************************************************************
 * TEST MODE 0: Full version with MixingOutput
 ******************************************************************************/
class PWMSim : public ModuleBase<PWMSim>, public OutputModuleInterface
{
public:
	PWMSim(bool hil_mode_enabled);
	~PWMSim() override;

	static int task_spawn(int argc, char *argv[]);
	static int custom_command(int argc, char *argv[]);
	static int print_usage(const char *reason = nullptr);
	int print_status() override;

	bool updateOutputs(uint16_t outputs[MAX_ACTUATORS],
			   unsigned num_outputs, unsigned num_control_groups_updated) override;

private:
	void Run() override;

	static constexpr uint16_t PWM_SIM_DISARMED_MAGIC = 900;
	static constexpr uint16_t PWM_SIM_FAILSAFE_MAGIC = 600;
	static constexpr uint16_t PWM_SIM_PWM_MIN_MAGIC = 1000;
	static constexpr uint16_t PWM_SIM_PWM_MAX_MAGIC = 2000;

	MixingOutput _mixing_output;
	uORB::SubscriptionInterval _parameter_update_sub;

	uORB::Publication<actuator_outputs_s> _actuator_outputs_sim_pub{ORB_ID(actuator_outputs_sim)};

	perf_counter_t	_cycle_perf{nullptr};
	perf_counter_t	_interval_perf{nullptr};
};
#endif
