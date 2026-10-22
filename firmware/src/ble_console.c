/* SPDX-License-Identifier: MIT */
#pragma clang diagnostic push
#pragma clang diagnostic error "-Wall"
#pragma clang diagnostic error "-Wextra"
/* S1 bench interface over Felucca's CDC console. Main context only. */
static ble_hw_diag_t ble_frozen_hw;
static ble_diag_t ble_frozen_ctl;
static ble_timing_diag_t ble_frozen_timing;
static uint8_t ble_console_frozen;

static void con_ble_bytes(const uint8_t *p,unsigned n)
{
    unsigned i;
    for (i=0;i<n;i++) { con_hex(p[i],2); if (i+1<n) con_putc(' '); }
    con_puts("\r\n");
}
static void con_ble_field(const char *name,uint32_t value)
{
    con_putc(' '); con_puts(name); con_putc(' '); con_dec((int32_t)value);
}
#if FELUCCA_BLE_STAGE >= 3 && FM1B_BLE_DIAG
static void con_ble_mic_dump(void)
{
    /* Snapshot before USB output yields: failures cannot tear a readout. */
    ble_mic_record_t records[BLE_MIC_RECORDS];
    unsigned count=ble_security_mic_records(records);
    con_kv("mic_records",(int32_t)count);
    for(unsigned i=0;i<count;i++) {
        const ble_mic_record_t *d=&records[i];
        con_puts("mic_record serial "); con_dec((int32_t)d->serial);
        con_puts(" ms "); con_dec((int32_t)d->ms);
        con_puts(" epoch "); con_dec((int32_t)d->epoch);
        con_puts(" received_ms "); con_dec((int32_t)d->received_ms);
        con_puts(" event "); con_dec(d->event);
        con_puts(" service_event "); con_dec(d->service_event);
        con_puts(" interval "); con_dec(d->interval);
        con_puts(" since_update_ms "); con_hex(d->since_update_ms,8); con_putc('\n');
        con_puts("mic_pdu serial "); con_dec((int32_t)d->serial);
        con_puts(" slot "); con_dec(d->slot);
        con_puts(" flags "); con_hex(d->flags,2);
        con_puts(" status "); con_hex(d->status,4);
        con_puts(" llid "); con_dec(d->header&3u);
        con_puts(" sn "); con_dec((d->header>>3)&1u);
        con_puts(" nesn "); con_dec((d->header>>2)&1u);
        con_puts(" md "); con_dec((d->header>>4)&1u);
        con_puts(" length "); con_dec(d->header>>8);
        con_puts(" counter "); con_hex((uint32_t)(d->counter>>32),2); con_hex((uint32_t)d->counter,8); con_putc('\n');
        for(unsigned j=0;j<d->rx_count+d->tx_count;j++) {
            int rx=j<d->rx_count;
            const ble_mic_pdu_t *p=rx ? &d->rx[j] : &d->tx[j-d->rx_count];
            con_puts(rx ? "mic_rx serial " : "mic_tx serial "); con_dec((int32_t)d->serial);
            con_puts(" index "); con_dec((int32_t)(rx ? j : j-d->rx_count));
            con_puts(" sn "); con_dec(p->sn);
            con_puts(" counter "); con_hex((uint32_t)(p->counter>>32),2); con_hex((uint32_t)p->counter,8);
            con_puts(" length "); con_dec(p->length); con_putc('\n');
        }
    }
}
#endif
static void con_ble_status(void)
{
#if FELUCCA_BLE_STAGE >= 3 && FM1B_BLE_DIAG
    con_ble_mic_dump();
#endif
    ble_hw_diag_t hw;
    ble_diag_t d;
    unsigned i;
    if (ble_console_frozen) { hw=ble_frozen_hw; d=ble_frozen_ctl; }
    else {
        uint32_t k=fm1_ble_irq_save(); hw=br_diag; fm1_ble_irq_restore(k);
        ble_get_diag(&d);
    }
#if FELUCCA_BLE_STAGE >= 3
#if FELUCCA_BLE_S31
    con_puts("BLE S3.1 / NimBLE 1.9.0 / MIDI; SC Just Works + legacy\r\n");
#else
    con_puts("BLE S3 / NimBLE 1.9.0 / MIDI; SC Just Works + legacy\r\n");
#endif
    {
        ble_security_diag_t enc; ble_seed_diag_t seed; ble_bond_diag_t bond;
        ble_security_get_diag(&enc); ble_seed_get_diag(&seed); ble_bonds_get_diag(&bond);
        con_kv("enc_phase_N_S1_S2_PA_PB_PC",enc.phase); con_kv("enc_rx_on",enc.rx_on); con_kv("enc_tx_on",enc.tx_on);
        con_kv("enc_enabled",enc.encrypted); con_kv("enc_starts",(int32_t)enc.starts); con_kv("enc_refreshes",(int32_t)enc.refreshes);
        con_kv("mic_failures",(int32_t)enc.mic_failures); con_kv("enc_phase_errors",(int32_t)enc.phase_errors);
        con_kv("ltk_requests",(int32_t)enc.ltk_requests); con_kv("ltk_negative",(int32_t)enc.negative_replies);
        con_kv("enc_tx_counter_hi",(int32_t)(enc.tx_counter>>32)); con_kx("enc_tx_counter_lo",(uint32_t)enc.tx_counter);
        con_kv("enc_rx_counter_hi",(int32_t)(enc.rx_counter>>32)); con_kx("enc_rx_counter_lo",(uint32_t)enc.rx_counter);
        con_kv("seed_FAILED_FIRST_CHAINED_PROVISIONED_UNPERSISTED",seed.state); con_kv("seed_origin",seed.origin);
        con_kv("seed_STORED_NOT_STORED_UNVERIFIED",seed.persistence); con_kx("seed_sequence",seed.sequence);
        con_kv("seed_credit_bits",seed.credit); con_kv("seed_malformed",(int32_t)seed.malformed); con_kv("seed_conflicts",(int32_t)seed.conflicts);
        con_kv("seed_flash_failures",(int32_t)seed.flash_failures); con_kv("le_rand_calls",(int32_t)seed.rand_calls);
        con_kv("entropy_bytes",(int32_t)seed.bytes); con_kv("reseed_attempts",(int32_t)seed.reseed_attempts);
        con_kv("bond_records",bond.records); con_kv("bond_pending",bond.pending); con_kv("bond_writes",(int32_t)bond.writes);
        con_kv("bond_erases",(int32_t)bond.erases); con_kv("bond_failures",(int32_t)bond.failures); con_kv("bond_malformed",(int32_t)bond.malformed);
        con_kv("bond_private_refused",(int32_t)bond.private_refused);
        con_kv("bond_private_dropped",(int32_t)bond.private_dropped);
        con_kv("bond_orphan_dropped",(int32_t)bond.orphan_dropped);
        con_kv("bond_equal_writes_skipped",(int32_t)bond.net_noops);
    }
#if FELUCCA_BLE_S31
    {
        ble_privacy_diag_t privacy; ble_privacy_get_diag(&privacy);
        con_kv("rl_capacity",privacy.capacity); con_kv("rl_entries",privacy.count); con_kv("rl_enabled",privacy.enabled);
        con_kv("rl_attempts",(int32_t)privacy.attempts); con_kv("rl_matches",(int32_t)privacy.matches);
        con_kv("rl_unresolved",(int32_t)privacy.unresolved); con_kv("rl_aes_failures",(int32_t)privacy.aes_failures);
        con_kv("rl_network_rejected",(int32_t)privacy.network_rejected);
    }
#endif
#else
    con_puts("BLE S2.1 / NimBLE 1.9.0 / MIDI; encryption off\r\n");
#endif
    {
        ble_timing_diag_t timing;
        if(ble_console_frozen) timing=ble_frozen_timing; else ble_time_get_diag(&timing);
        ble_breadcrumb_t b; uint32_t timer[8],k=fm1_ble_irq_save();
        b=fm1_ble_breadcrumb; fm1_ble_worker_timer_readback(timer);
        timer[4]=br_diag.worker_timer_calls; timer[5]=br_diag.worker_timer_gap_ticks;
        timer[6]=br_diag.worker_timer_over_gap; timer[7]=br_diag.worker_timer_last_ticks;
        fm1_ble_irq_restore(k);
        con_kv("executor_selftest",(int32_t)hw.executor_selftest);
        con_kv("executor_dispatch_max_us",(int32_t)(timing.dispatch_max/24u));
        con_kv("executor_dispatch_live_max_us",(int32_t)(timing.dispatch_live_max/24u));
        con_kv("executor_dispatch_negative",(int32_t)timing.dispatch_negative);
        con_kv("executor_wall_max_us",(int32_t)(timing.wall_max/24u));
        con_kv("executor_own_max_us",(int32_t)(timing.own_max/24u));
        con_kv("executor_item_max_us",(int32_t)(timing.item_max/24u));
        con_kv("executor_item_over_100us",(int32_t)timing.item_over_100us);
        con_puts("executor_item_max_kind "); con_puts(ble_time_kind_name(timing.item_kind)); con_puts("\r\n");
        con_kx("executor_item_max_opcode",timing.item_opcode);
        for(i=0;i<BLE_WORK_COUNT;i++) {
            con_puts("executor_kind "); con_puts(ble_time_kind_name(i));
            con_puts(" max_ticks "); con_dec((int32_t)timing.kind_max[i]);
            con_puts(" count "); con_dec((int32_t)timing.kind_count[i]);
            con_puts(" over100 "); con_dec((int32_t)timing.kind_over_100us[i]);
            con_puts(" opcode "); con_hex(timing.kind_opcode[i],4); con_puts("\r\n");
        }
        con_kv("executor_own_over_250us",(int32_t)timing.own_over_250us);
        con_kv("executor_over_250us_plus_item",(int32_t)timing.pass_over_budget);
        con_kv("refill_max_us",(int32_t)(timing.refill_max/24u));
        con_kv("refill_ccm_max_us",(int32_t)(timing.refill_ccm_max/24u));
        con_kv("refill_other_max_us",(int32_t)(timing.refill_other_max/24u));
        con_kv("refill_over_80us",(int32_t)timing.refill_over_80us);
        con_kv("worker_timer_calls",(int32_t)timer[4]);
        con_kv("worker_timer_max_gap_us",(int32_t)(timer[5]/24u));
        con_kv("worker_timer_over_7500us",(int32_t)timer[6]);
        con_kx("worker_timer_last_ticks",timer[7]);
        con_kx("worker_timer_con",timer[0]); con_kv("worker_timer_cnt",(int32_t)timer[1]);
        con_kv("worker_timer_prd",(int32_t)timer[2]); con_kx("worker_timer_irqcfg",timer[3]);
        con_kx("ble_breadcrumb",b.last); con_kx("ble_breadcrumb_ticks",b.last_ticks);
        con_kx("ble_fault_breadcrumb",b.fault); con_kx("ble_fault_ticks",b.fault_ticks);
        con_kv("ble_fault_crash_count",(int32_t)b.fault_count);
    }
    {
        ble_link_diag_t link; ble_host_diag_t host;
        ble_ll_get_diag(&link); ble_host_get_diag(&host);
        con_kv("link_state_idle_created_up_closing_stopped",link.state);
        con_kv("link_epoch",(int32_t)link.epoch); con_kv("link_established",link.established);
        con_kv("conn_interval_1250us",link.interval); con_kv("conn_latency",link.latency);
        con_kv("conn_timeout_10ms",link.timeout); con_kv("conn_event",link.event);
        con_kv("connections",(int32_t)link.created); con_kv("connect_invalid",(int32_t)link.invalid);
        con_kv("conn_events",(int32_t)link.events);
        con_kv("rx_A_new",(int32_t)link.rx_class[0]); con_kv("rx_B_crc_5_13",(int32_t)link.rx_class[1]);
        con_kv("rx_C_repeat_9",(int32_t)link.rx_class[2]); con_kv("rx_D_unknown",(int32_t)link.rx_class[3]);
        con_kv("retransmission_rx",(int32_t)link.repetitions);
        con_kv("rx_delayed_stamp",(int32_t)link.rx_delayed);
        con_kv("rx_backpressure",(int32_t)link.rx_backpressure); con_kv("rx_queued",link.rx_used); con_kv("rx_peak",link.rx_peak);
        con_kv("tx_queued_loaded",link.tx_used); con_kv("establishment_fail",(int32_t)link.establishment_fail);
        con_kv("supervision_fail",(int32_t)link.supervision_fail); con_kv("disconnects",(int32_t)link.disconnects);
        con_kx("last_disconnect_reason",link.reason); con_kv("conn_updates",(int32_t)link.updates);
        con_kv("channel_map_updates",(int32_t)link.map_updates); con_kv("late_instants",(int32_t)link.late_instants);
        con_puts("pending_proc"); con_ble_field("kind",link.instant_kind);
        con_ble_field("instant",link.instant); con_ble_field("received",link.received);
        con_ble_field("programmed",link.programmed); con_ble_field("interval",link.pending.interval);
        con_ble_field("latency",link.pending.latency); con_ble_field("timeout",link.pending.timeout);
        con_ble_field("win",link.pending.win); con_ble_field("offset",link.pending.offset);
        con_puts(" map "); con_ble_bytes(link.pending.map,5);
        con_puts("control_last_ignored"); con_ble_field("opcode",link.ignored_opcode);
        con_ble_field("reason",link.ignored_reason); con_ble_field("event",link.ignored_event); con_puts("\r\n");
        for(i=1;i<BLE_IGNORE_COUNT;i++) {
            static const char *const names[]={"none","length","params","map","pending","role","unexpected","version","reserved","encrypted"};
            con_puts("control_ignore_"); con_puts(names[i]); con_putc(' ');
            con_dec((int32_t)link.ignore_reasons[i]); con_puts("\r\n");
        }
        for(i=0;i<4;i++) {
            const ble_update_diag_t *u=&link.update[i];
            con_puts("conn_update"); con_ble_field("serial",u->serial); con_ble_field("epoch",u->epoch);
            con_ble_field("old",u->old_interval); con_ble_field("new",u->new_interval);
            con_ble_field("win",u->win); con_ble_field("offset",u->offset);
            con_ble_field("instant",u->instant); con_ble_field("received",u->received);
            con_ble_field("outcome",u->outcome); con_ble_field("reason",u->reason); con_puts("\r\n");
        }
        con_kv("acl_tx_accepted",(int32_t)link.acl_in); con_kv("acl_tx_rejected",(int32_t)link.acl_rejected);
        con_kv("acl_rx",(int32_t)link.acl_out); con_kv("acl_acked",(int32_t)link.acknowledged);
        con_kv("ncp_credits",(int32_t)link.ncp); con_kv("acl_flushed",(int32_t)link.flushed);
        con_kv("control_dropped",(int32_t)link.ctrl_drop); con_kv("control_ignored",(int32_t)link.ignored);
        con_kv("worker_max_gap_us",(int32_t)(link.worker_gap_ticks/24u));
        con_kv("worker_max_us",(int32_t)(link.worker_max_ticks/24u));
        con_kv("worker_over_7500us",(int32_t)link.worker_over_gap);
        con_kv("worker_calls",(int32_t)link.worker_calls); con_kv("terminate_timeouts",(int32_t)link.terminate_timeouts);
        con_kv("nimble_running",host.running); con_kv("nimble_synced",host.synced);
        con_kv("nimble_syncs",(int32_t)host.syncs); con_kv("nimble_resets",(int32_t)host.resets);
        con_kv("nimble_error",(int32_t)host.error); con_kv("nimble_assertions",(int32_t)host.assertions);
        con_kv("nimble_event_drop",(int32_t)host.event_drop); con_kv("nimble_acl_drop",(int32_t)host.acl_drop);
#if FELUCCA_BLE_STAGE >= 3
        con_kv("nimble_encrypted",host.encrypted); con_kv("nimble_bonded",host.bonded); con_kv("nimble_key_size",host.key_size);
        con_kv("nimble_pair_method_legacy1_SC2",host.pair_method); con_kv("security_requests",(int32_t)host.security_requests);
        con_kv("nimble_bond_sc",host.sc); con_kv("nimble_bond_identity_known",host.identity_known);
        con_kv("nimble_peer_identity_type",host.peer_identity_type); con_puts("nimble_peer_identity "); con_ble_bytes(host.peer_identity,6);
        con_kv("security_skipped",(int32_t)host.security_skipped); con_kv("security_already",(int32_t)host.security_already);
        con_kv("pair_failures",(int32_t)host.pair_failures);
        con_kv("pair_last_status",(int32_t)host.pair_status);
#endif
        con_kv("nimble_main_gap_ms",(int32_t)host.main_gap_ms); con_kv("nimble_mtu",host.mtu);
        con_kv("midi_echo",host.echo); con_kv("nimble_subscribed",host.subscribed); con_kv("nimble_heap_used",(int32_t)host.heap_used);
        con_kv("nimble_heap_peak",(int32_t)host.heap_peak); con_kv("nimble_heap_fail",(int32_t)host.heap_fail);
        con_kv("midi_packets_in",(int32_t)host.midi_in); con_kv("midi_packets_out",(int32_t)host.midi_out);
        con_kv("midi_bad",(int32_t)host.midi_bad); con_kv("midi_drop",(int32_t)host.midi_drop);
    }
    con_kv("frozen",ble_console_frozen);
    con_kv("loop_iters_per_us_x1000",(int32_t)hw.loop_iters_per_us_x1000); con_kv("loop_ticks",(int32_t)hw.loop_ticks);
    for (i=0;i<4;i++) { con_puts("clock "); con_dec((int32_t)i); con_putc(' '); con_hex(hw.clocks[i],8); con_puts("\r\n"); }
    con_puts("icfg_probe ");
    for (i=0;i<4;i++) { con_hex(hw.irq_probe[i],8); con_putc(i==3 ? '\r' : ' '); }
    con_putc('\n');
    con_kv("ticks_while_irq_masked",(int32_t)hw.irq_ticks_while_masked);
    con_kv("error",(int32_t)hw.error); con_kv("failure_stage",(int32_t)hw.failure_stage); con_kv("calibration_1_record_2_self",(int32_t)hw.calibration);
    con_kv("vm_status",ble_cal.vm_status); con_kv("btif_status",ble_cal.btif_status);
    con_kx("records_present",ble_cal.present);
    con_puts("public_address_hci "); con_ble_bytes(ble_cal.public_addr,6);
    con_puts("configured_random_hci "); con_ble_bytes(ble_random_config,6);
    con_kx("trim_crc",ble_get32(ble_cal.trim+64));
    con_kv("bringup_us",(int32_t)(hw.bringup_ticks/24));
    con_kx("bringup_end_ticks",hw.bringup_end_ticks);
    con_kx("first_hci_command_ticks",d.first_command_ticks);
    for (i=0;i<15;i++) { con_puts("step_us "); con_dec((int32_t)i+1); con_putc(' '); con_dec((int32_t)(hw.step_ticks[i]/24)); con_puts("\r\n"); }
    con_kv("enabled",d.enabled); con_kv("stopping",d.stopping);
    con_kv("driver_step",d.driver_step); con_kv("driver_error",d.driver_error);
    con_kv("commands",(int32_t)d.commands); con_kv("replies",(int32_t)d.replies);
    con_kx("last_opcode",d.last_opcode); con_kx("last_status",d.last_status);
    con_kv("intake_rejected",(int32_t)d.rejected); con_kv("command_overflow",(int32_t)d.overflow);
    con_kv("advertising_events",(int32_t)hw.adv_events);
    con_kv("event_irqs",(int32_t)hw.event_irqs); con_kv("rx_irqs",(int32_t)hw.rx_irqs);
    con_kv("scan_req",(int32_t)hw.scan_req); con_kv("connect_ignored",(int32_t)hw.connect_ignored);
    con_kv("bad_rx",(int32_t)hw.bad_rx);
    con_kv("rx_max_ticks",(int32_t)hw.rx_max_ticks); con_kv("event_max_ticks",(int32_t)hw.event_max_ticks);
    con_kv("rx_over_380us",(int32_t)hw.irq_over_budget);
    con_kv("event_work_max_ticks",(int32_t)hw.event_work_max_ticks);
    con_kv("event_work_over_150us",(int32_t)hw.event_over_150us);
    con_kv("ble_critical_max_ticks",(int32_t)hw.critical_max_ticks);
    con_kv("ble_critical_over_390us",(int32_t)hw.critical_over_390us);
    con_kv("main_controller_gap_us",(int32_t)(hw.worker_gap_ticks/24));
    con_kv("stop_timeouts",(int32_t)hw.stop_timeouts); con_kv("restarts",(int32_t)hw.restarts);
    con_kx("enable_ticks",hw.enable_ticks); con_kx("first_event_close_ticks",hw.first_event_ticks);
    con_kx("dma_base",hw.dma_base); con_kx("dma_end",hw.dma_end); con_kx("modem_base",hw.modem_base);
    con_kv("dma_canary_errors",(int32_t)hw.canary_errors); con_kv("p33_timeouts",(int32_t)hw.p33_timeouts);
    con_kv("trace_records",(int32_t)ble_trace_size()); con_kv("trace_total",(int32_t)d.trace_count);
    con_kv("trace_frozen_drops",(int32_t)d.trace_frozen_drops);
    con_puts("vco_limits "); con_ble_bytes(hw.vco,48);
}
static int con_ble_hexbyte(const char **p,uint8_t *v)
{
    unsigned j,n=0;
    const char *s=*p;
    while (*s==' ' || *s==':') s++;
    for (j=0;j<2;j++) {
        char c=*s++;
        unsigned d;
        if (c>='0' && c<='9') d=(unsigned)(c-'0');
        else if (c>='a' && c<='f') d=(unsigned)(c-'a'+10);
        else if (c>='A' && c<='F') d=(unsigned)(c-'A'+10);
        else return 0;
        n=n*16+d;
    }
    *p=s; *v=(uint8_t)n; return 1;
}
static void con_ble(const char *p)
{
    if(con_word(&p,"echo")) {
        ble_host_echo(con_word(&p,"on")); con_puts("BLE MIDI echo configured\r\n");
    } else if(con_word(&p,"host")) {
        ble_host_resume(); con_puts("NimBLE resynchronisation queued\r\n");
    } else if (con_word(&p,"rate")) {
        br_clock_probe(); con_puts("loop throughput sampled; use ble for result (not Hz)\r\n");
    } else if (con_word(&p,"pmu")) {
        unsigned i;
        for (i=0;i<38;i++) {
            unsigned index=i<24 ? i : i<30 ? i-24+0x30 : i-30+0x70;
            con_hex(index,2); con_putc(' '); con_hex(br_diag.p33_before[i],2);
            con_putc(' '); con_hex(br_diag.p33_after[i],2); con_puts("\r\n");
        }
    } else if (con_word(&p,"regs")) {
        uint32_t r[50]; unsigned i;
        fm1_ble_readback(r);
        con_puts("WLA0..30 WL0..7 WF0..9 BSB (live read)\r\n");
        for (i=0;i<50;i++) { con_dec((int32_t)i); con_putc(' '); con_hex(r[i],8); con_puts("\r\n"); }
    } else if (con_word(&p,"freeze")) {
        uint32_t k=fm1_ble_irq_save();
        ble_trace_freeze(1); ble_get_diag(&ble_frozen_ctl); ble_frozen_hw=br_diag;
        ble_time_get_diag(&ble_frozen_timing);
        ble_console_frozen=1; fm1_ble_irq_restore(k);
        con_kv("frozen_records",(int32_t)ble_trace_size());
    } else if (con_word(&p,"thaw")) {
        ble_trace_freeze(0); ble_console_frozen=0; con_puts("trace running\r\n");
    } else if (con_word(&p,"trace")) {
        int ok,ok2; uint32_t start=con_num(&p,&ok),n=con_num(&p,&ok2),i;
        if (!ok) start=0;
        if (!ok2 || n>32) n=8;
        if (!ble_console_frozen) { con_puts("ble freeze first\r\n"); return; }
        for (i=0;i<n;i++) {
            const ble_trace_t *r=ble_trace_record(start+i);
            if (!r) break;
            con_dec((int32_t)(start+i)); con_putc(' '); con_dec((int32_t)r->ms); con_putc(' ');
            con_hex(r->dir_type,2); con_putc(' '); con_dec(r->len); con_putc(' ');
            con_hex(r->intake,2); con_putc(' '); con_ble_bytes(r->data,r->len<32 ? r->len : 32);
        }
    } else if (con_word(&p,"adv")) {
        ble_host_manual(); ble_driver_start(0,ble_random_config); con_puts("ADV_IND startup queued\r\n");
    } else if (con_word(&p,"nonconn")) {
        ble_host_manual(); ble_driver_start(3,ble_random_config); con_puts("ADV_NONCONN_IND startup queued\r\n");
    } else if (con_word(&p,"random")) {
        uint8_t addr[6]; unsigned i;
        for (i=0;i<6;i++) if (!con_ble_hexbyte(&p,&addr[5-i])) break;
        while (*p==' ') p++;
        if (i!=6 || *p || !ble_random_valid(addr)) { con_puts("usage: ble random XX:XX:XX:XX:XX:XX (static, unique)\r\n"); return; }
        memcpy(ble_random_config,addr,6); con_puts("RAM identity set; ble adv to start\r\n");
    } else if (con_word(&p,"power")) {
        int ok; uint32_t level=con_num(&p,&ok);
        if (!ok || level>11) con_puts("usage: ble power 0..11\r\n");
        else { ble_set_power((uint8_t)level); con_puts("power level queued\r\n"); }
    } else if (con_word(&p,"stop")) {
        static const uint8_t cmd[4]={10,32,1,0};
        ble_host_manual(); ble_driver_stop(); con_kv("hci_intake",ble_hci_send(1,cmd,4));
    } else if (con_word(&p,"reset")) {
        static const uint8_t cmd[3]={3,12,0};
        ble_host_manual(); ble_driver_stop(); con_kv("hci_intake",ble_hci_send(1,cmd,3));
    } else if (con_word(&p,"hci")) {
        uint8_t packet[BLE_HCI_MAX]; unsigned n=0;
        while (*p==' ') p++;
        while (*p && n<sizeof packet && con_ble_hexbyte(&p,&packet[n])) { n++; while (*p==' ') p++; }
        if (*p) { con_puts("invalid hex or packet too long\r\n"); return; }
        ble_host_manual(); ble_driver_stop(); con_kv("hci_intake",ble_hci_send(1,packet,(uint16_t)n));
    } else if (!*p) con_ble_status();
    else con_puts("ble [echo on/off|host|rate|pmu|regs|adv|nonconn|stop|reset|power N|random ADDR|hci HEX|freeze|trace START N|thaw]\r\n");
}

#pragma clang diagnostic pop
