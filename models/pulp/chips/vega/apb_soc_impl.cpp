/*
 * Copyright (C) 2018 ETH Zurich and University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * Authors: Germain Haugou, ETH (germain.haugou@iis.ee.ethz.ch)
 */

#include <vp/vp.hpp>
#include <vp/itf/io.hpp>
#include <vp/itf/wire.hpp>
#include <stdio.h>
#include <string.h>

#include "archi/chips/vega/apb_soc_ctrl.h"
#include "archi/chips/vega/apb_soc.h"

class apb_soc_ctrl : public vp::component
{

public:

  apb_soc_ctrl(const char *config);

  int build();
  void start();
  void reset(bool active);


  static vp::io_req_status_e req(void *__this, vp::io_req *req);

private:
  vp::io_req_status_e sleep_ctrl_req(int reg_offset, int size, bool is_write, uint8_t *data);

  static void bootsel_sync(void *__this, int value);
  static void confreg_ext_sync(void *__this, uint32_t value);
  static void wakeup_rtc_sync(void *__this, bool wakeup);
  static void wakeup_gpio_sync(void *__this, int value, int gpio);
  void set_wakeup(int value);

  vp::trace     trace;
  vp::io_slave in;

  vp::wire_slave<int>   bootsel_itf;
  vp::wire_master<uint32_t> bootaddr_itf;
  vp::wire_master<bool> cluster_reset_itf;
  vp::wire_master<bool> cluster_power_itf;
  vp::wire_master<bool> cluster_power_irq_itf;
  vp::wire_master<bool> cluster_clock_gate_irq_itf;
  vp::wire_master<int>  event_itf;
  vp::wire_slave<bool>  wakeup_rtc_itf;
  vp::wire_master<bool>  wakeup_out_itf;
  vp::wire_master<unsigned int>  wakeup_seq_itf;

  std::vector<vp::wire_slave<int>> wakeup_gpio_itf;

  vp::wire_master<uint32_t> confreg_soc_itf;
  vp::wire_slave<uint32_t> confreg_ext_itf;

  int cluster_power_event;
  int cluster_clock_gate_event;

  uint32_t core_status;
  uint32_t bootaddr;
  uint32_t pmu_bypass;
  bool cluster_reset;
  bool cluster_power;
  bool cluster_clock_gate;

  unsigned int extwake_sel;
  unsigned int extwake_type;
  unsigned int extwake_en;
  unsigned int cfg_wakeup;
  unsigned int extwake_sync;
  unsigned int boot_type;

  vp_apb_soc_safe_pmu_sleepctrl   r_sleep_ctrl;

  vp::reg_32     jtag_reg_ext;
  vp::reg_32     r_bootsel;

  int wakeup;
};

apb_soc_ctrl::apb_soc_ctrl(const char *config)
: vp::component(config)
{

}

void apb_soc_ctrl::set_wakeup(int value)
{
  if (this->wakeup == 0)
  {
    this->wakeup = value;
    this->wakeup_out_itf.sync(value);
  }
}



vp::io_req_status_e apb_soc_ctrl::sleep_ctrl_req(int reg_offset, int size, bool is_write, uint8_t *data)
{
  this->r_sleep_ctrl.access(reg_offset, size, data, is_write);

  if (is_write)
  {
    this->trace.msg("Modified SLEEP_CTRL (reboot: %d, smartwake_en: %d, rtcwake_en: %d, extwake_type: %d, extwake_en: %d, mram_wakestate: %d, cluster_wakestate: %d, ret_mem: 0x%4.4x)\n",
      this->r_sleep_ctrl.reboot_get(),
      this->r_sleep_ctrl.smartwake_en_get(),
      this->r_sleep_ctrl.rtcwake_en_get(),
      this->r_sleep_ctrl.extwake_type_get(),
      this->r_sleep_ctrl.extwake_en_get(),
      this->r_sleep_ctrl.mram_wakestate_get(),
      this->r_sleep_ctrl.cluster_wakestate_get(),
      this->r_sleep_ctrl.ret_mem_get()
    );
  }
  else
  { 
    this->trace.msg("Clearing wakeup signal\n");

    this->wakeup = 0;
  }

  return vp::IO_REQ_OK;

#if 0
    if (is_write)
    {
      uint32_t value = *(uint32_t *)data;
      _this->extwake_sel = (value >> 6) & 0x1f;   // GPIO selection for wakeup
      _this->extwake_type = (value >> 11) & 0x3;  // GPIO wakeup type (raising edge, etc)
      _this->extwake_en = (value >> 13) & 0x1;    // GPIO wakeup enabled
      _this->cfg_wakeup = (value >> 14) & 0x3;    // PMU sequence used to wakeup
      _this->boot_type = (value >> 18) & 0x3;
      _this->wakeup_seq_itf.sync(_this->cfg_wakeup);
    }
    else
    {
      _this->set_wakeup(0);
      *(uint32_t *)data = (_this->extwake_sel << 6) | (_this->extwake_type << 11) | (_this->extwake_en << 13) | (_this->cfg_wakeup << 14) | (_this->boot_type << 18);
    }
#endif

}



vp::io_req_status_e apb_soc_ctrl::req(void *__this, vp::io_req *req)
{
  apb_soc_ctrl *_this = (apb_soc_ctrl *)__this;

  uint64_t offset = req->get_addr();
  uint8_t *data = req->get_data();
  uint64_t size = req->get_size();
  bool is_write = req->get_is_write();

  vp::io_req_status_e err = vp::IO_REQ_OK;

  _this->trace.msg("Apb_soc_ctrl access (offset: 0x%x, size: 0x%x, is_write: %d)\n", offset, size, is_write);

  if (size != 4) return vp::IO_REQ_INVALID;

  int reg_id = offset / 4;
  int reg_offset = offset % 4;

  if (offset == APB_SOC_CORESTATUS_OFFSET)
  {
    if (!is_write)
    {
      *(uint32_t *)data = _this->core_status;
    }
    else
    {
      // We are writing to the status register, the 31 LSBs are the return value of the platform and the last bit
      // makes the platform exit when set to 1
      _this->core_status = *(uint32_t *)data;
      
      if ((_this->core_status >> APB_SOC_STATUS_EOC_BIT) & 1) 
      {
        _this->clock->stop_engine(_this->core_status & 0x7fffffff);
      }
      else
      {
        uint32_t value = *(uint32_t *)data;
        if (value == 0x0BBAABBA)
        {
          _this->power.get_engine()->start_capture();
        }
        else if (value == 0x0BBADEAD)
        {
          _this->power.get_engine()->stop_capture();
        }
      }
    }
  }
  else if (offset == APB_SOC_JTAG_REG)
  {
    if (is_write)
    {
      _this->confreg_soc_itf.sync(*(uint32_t *)data);
    }
    else
    {
      *(uint32_t *)data = _this->jtag_reg_ext.get() << APB_SOC_JTAG_REG_EXT_BIT;
    }
  }
  else if (offset == APB_SOC_SLEEP_CTRL_OFFSET || offset == APB_SOC_SAFE_PMU_SLEEPCTRL_OFFSET)
  {
    err = _this->sleep_ctrl_req(reg_offset, size, is_write, data);
  }
  else if (offset == APB_SOC_PADS_CONFIG)
  {
    int reg_offset = offset % 4;

    _this->r_bootsel.access(reg_offset, size, data, is_write);
  }
  else if (offset == APB_SOC_BOOTADDR_OFFSET)
  {
    if (is_write)
    {
      _this->trace.msg("Setting boot address (addr: 0x%x)\n", *(uint32_t *)data);
      if (_this->bootaddr_itf.is_bound())
        _this->bootaddr_itf.sync(*(uint32_t *)data);
      
      _this->bootaddr = *(uint32_t *)data;
    }
    else *(uint32_t *)data = _this->bootaddr;
  }
  else if (offset == APB_SOC_BYPASS_OFFSET)
  {
    if (is_write)
    {
      _this->trace.msg("Setting PMU bypass (addr: 0x%x)\n", *(uint32_t *)data);

      _this->pmu_bypass = *(uint32_t *)data;

      bool new_cluster_power = (_this->pmu_bypass >> 3) & 1;
      bool new_cluster_reset = (_this->pmu_bypass >> 13) & 1;
      bool new_cluster_clock_gate = (_this->pmu_bypass >> 10) & 1;

      if (_this->cluster_reset != new_cluster_reset)
      {
        if (_this->cluster_reset_itf.is_bound())
        {
          _this->cluster_reset_itf.sync(~new_cluster_reset);
        }

        _this->cluster_reset = new_cluster_reset;
      }

      if (_this->cluster_power != new_cluster_power)
      {
        _this->trace.msg("Setting cluster power (power: 0x%d)\n", new_cluster_power);

        if (_this->cluster_power_itf.is_bound())
        {
          _this->cluster_power_itf.sync(new_cluster_power);
        }

        _this->trace.msg("Triggering soc event (event: 0x%d)\n", _this->cluster_power_event);
        _this->event_itf.sync(_this->cluster_power_event);

        if (_this->cluster_power_irq_itf.is_bound())
        {
          _this->cluster_power_irq_itf.sync(true);
        }
      }

      if (_this->cluster_clock_gate != new_cluster_clock_gate)
      {
        _this->trace.msg("Triggering soc event (event: 0x%d)\n", _this->cluster_clock_gate_event);
        _this->event_itf.sync(_this->cluster_clock_gate_event);

        if (_this->cluster_clock_gate_irq_itf.is_bound())
        {
          _this->cluster_clock_gate_irq_itf.sync(true);
        }
      }

      _this->cluster_power = new_cluster_power;
      _this->cluster_clock_gate = new_cluster_clock_gate;

    }
    else
    {
      *(uint32_t *)data = _this->pmu_bypass;
    }
  }
  else
  {

  }


  return err;
}


void apb_soc_ctrl::bootsel_sync(void *__this, int value)
{
  apb_soc_ctrl *_this = (apb_soc_ctrl *)__this;
  _this->r_bootsel.set(value);
}



void apb_soc_ctrl::wakeup_gpio_sync(void *__this, int value, int gpio)
{
  apb_soc_ctrl *_this = (apb_soc_ctrl *)__this;
  if (_this->extwake_en && gpio == _this->extwake_sel)
  {
    int old_value = _this->extwake_sync;

    _this->extwake_sync = value;

    switch (_this->extwake_type)
    {
      case 0: {
        if (old_value == 0 && _this->extwake_sync == 1)
          _this->set_wakeup(1);
        break;
      }
      case 1: {
        if (old_value == 1 && _this->extwake_sync == 0)
          _this->set_wakeup(1);
        break;
      }
      case 2: {
        if (_this->extwake_sync == 1)
          _this->set_wakeup(1);
        break;
      }
      case 3: {
        if (_this->extwake_sync == 0)
          _this->set_wakeup(1);
        break;
      }
    }
  }
}

void apb_soc_ctrl::wakeup_rtc_sync(void *__this, bool wakeup)
{
  apb_soc_ctrl *_this = (apb_soc_ctrl *)__this;
  if (wakeup && _this->r_sleep_ctrl.rtcwake_en_get())
  {
    _this->trace.msg("Received RTC wakeup\n");
    _this->set_wakeup(1);
  }
}

void apb_soc_ctrl::confreg_ext_sync(void *__this, uint32_t value)
{
  apb_soc_ctrl *_this = (apb_soc_ctrl *)__this;
  _this->jtag_reg_ext.set(value);
}

int apb_soc_ctrl::build()
{
  traces.new_trace("trace", &trace, vp::DEBUG);
  in.set_req_meth(&apb_soc_ctrl::req);
  new_slave_port("input", &in);

  new_master_port("bootaddr", &this->bootaddr_itf);

  new_master_port("event", &event_itf);

  new_master_port("cluster_power", &cluster_power_itf);
  new_master_port("cluster_reset", &cluster_reset_itf);
  new_master_port("cluster_power_irq", &cluster_power_irq_itf);

  new_master_port("cluster_clock_gate_irq", &cluster_clock_gate_irq_itf);

  this->wakeup_rtc_itf.set_sync_meth(&apb_soc_ctrl::wakeup_rtc_sync);
  new_slave_port("wakeup_rtc", &this->wakeup_rtc_itf);

  this->wakeup_gpio_itf.resize(32);
  for (int i=0; i<32; i++)
  {
    this->wakeup_gpio_itf[i].set_sync_meth_muxed(&apb_soc_ctrl::wakeup_gpio_sync, i);
    new_slave_port("wakeup_gpio" + std::to_string(i), &this->wakeup_gpio_itf[i]);
  }

  new_master_port("wakeup_out", &this->wakeup_out_itf);

  new_master_port("wakeup_seq", &this->wakeup_seq_itf);

  confreg_ext_itf.set_sync_meth(&apb_soc_ctrl::confreg_ext_sync);
  this->new_slave_port("confreg_ext", &this->confreg_ext_itf);

  this->new_master_port("confreg_soc", &this->confreg_soc_itf);

  this->new_reg("jtag_reg_ext", &this->jtag_reg_ext, 0, false);

  bootsel_itf.set_sync_meth(&apb_soc_ctrl::bootsel_sync);
  new_slave_port("bootsel", &bootsel_itf);
  this->new_reg("bootsel", &this->r_bootsel, 0, false);

  cluster_power_event = this->get_js_config()->get("cluster_power_event")->get_int();
  cluster_clock_gate_event = this->get_js_config()->get("cluster_clock_gate_event")->get_int();

  core_status = 0;
  this->jtag_reg_ext.set(0);

  // This one is in the always-on domain and so it is reset only when the
  // component is powered-up
  this->wakeup = 0;
  this->extwake_sel = 0;
  this->extwake_type = 0;
  this->extwake_en = 0;
  this->cfg_wakeup = 0;
  this->boot_type = 0;
  this->extwake_sync = 0;


  this->new_reg("sleep_ctrl", &this->r_sleep_ctrl, 0, false);

  this->r_sleep_ctrl.set(0);

  return 0;
}

void apb_soc_ctrl::reset(bool active)
{
  if (active)
  {
    cluster_power = false;
    cluster_clock_gate = false;
  }
}

void apb_soc_ctrl::start()
{
}

extern "C" void *vp_constructor(const char *config)
{
  return (void *)new apb_soc_ctrl(config);
}
