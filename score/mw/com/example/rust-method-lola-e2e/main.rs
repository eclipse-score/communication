/********************************************************************************
 * Copyright (c) 2026 Contributors to the Eclipse Foundation
 *
 * See the NOTICE file(s) distributed with this work for additional
 * information regarding copyright ownership.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Apache License Version 2.0 which is available at
 * https://www.apache.org/licenses/LICENSE-2.0
 *
 * SPDX-License-Identifier: Apache-2.0
 ********************************************************************************/

//! Cross-process checks of the Rust method support on the LoLa runtime.
//!
//! `rust_method_lola_e2e producer <config>` offers `CalcInterface` and serves until killed.
//! `rust_method_lola_e2e consumer <config> <scenario>` runs one scenario against it and exits 0
//! only if every step gave the right answer. A watchdog ends the process with exit code 3 when a
//! step hangs, so a call that never returns is reported as a failure instead of stalling the run.
//! `run_test.sh` starts the processes for each scenario.

use std::future::ready;
use std::path::{Path, PathBuf};
use std::process::ExitCode;
use std::sync::Mutex;
use std::time::{Duration, Instant};

use futures::executor::block_on;
use rust_method_lola_e2e_gen::{CalcConsumer, CalcInterface, Operands, Total, Wide};
use score_com::{
    Builder, FindServiceSpecifier, InstanceSpecifier, LolaRuntimeBuilderImpl, LolaRuntimeImpl, Runtime, RuntimeBuilder,
    ServiceDiscovery,
};

const INSTANCE: &str = "/Test/Calc/Instance";
const SLOW_MS: u64 = 300;

// ---------------------------------------------------------------- watchdog

static STEP: Mutex<Option<(String, Instant)>> = Mutex::new(None);

/// Marks the start of a step that must finish within `limit`.
fn step(name: &str, limit: Duration) {
    println!("STEP {name}");
    if let Ok(mut s) = STEP.lock() {
        *s = Some((name.to_owned(), Instant::now() + limit));
    }
}

fn start_watchdog() {
    std::thread::spawn(|| loop {
        std::thread::sleep(Duration::from_millis(100));
        let expired = STEP
            .lock()
            .ok()
            .and_then(|s| s.as_ref().filter(|(_, d)| Instant::now() > *d).map(|(n, _)| n.clone()));
        if let Some(name) = expired {
            println!("HANG at step {name}");
            println!("RESULT: FAIL");
            std::process::exit(3);
        }
    });
}

// ---------------------------------------------------------------- shared

fn runtime(config: &Path) -> LolaRuntimeImpl {
    let mut builder = LolaRuntimeBuilderImpl::new();
    builder.load_config(config);
    builder.build().expect("runtime")
}

fn specifier() -> InstanceSpecifier {
    InstanceSpecifier::new(INSTANCE).expect("specifier")
}

fn wide(seed: u8) -> Wide {
    Wide {
        a: seed,
        b: 0x0102_0304_0506_0708 ^ u64::from(seed),
        c: 0xBEEF ^ u16::from(seed),
        d: f64::from(seed) + 0.25,
    }
}

/// What `mix` returns: every member combined, so a member at a wrong offset shows.
fn mixed(x: Wide, y: Wide) -> Wide {
    Wide {
        a: x.a.wrapping_add(y.a),
        b: x.b ^ y.b.rotate_left(8),
        c: x.c.wrapping_add(y.c),
        d: x.d * 2.0 + y.d,
    }
}

// ---------------------------------------------------------------- producer

fn producer(config: &Path) -> ExitCode {
    let rt = runtime(config);
    let producer = rt
        .producer_builder::<CalcInterface>(specifier())
        .build()
        .expect("producer");
    // reset(v) stores v, last() returns it.
    let stored: &'static Mutex<Total> = Box::leak(Box::new(Mutex::new(Total { value: 0 })));
    // Handlers get their arguments by reference and return a future of the result.
    let _offered = producer
        .init()
        .register_add_handler(|ops: &Operands| {
            ready(Total {
                value: ops.a.wrapping_add(ops.b),
            })
        })
        .register_mix_handler(|x: &Wide, y: &Wide| ready(mixed(*x, *y)))
        .register_ping_handler(|| ready(Total { value: 4242 }))
        .register_reset_handler(move |v: &Total| {
            if let Ok(mut slot) = stored.lock() {
                *slot = *v;
            }
            ready(())
        })
        .register_last_handler(move || ready(stored.lock().map(|slot| *slot).unwrap_or(Total { value: -1 })))
        .register_slow_handler(|v: &Total| {
            // -1 makes the handler panic, to see what a panic inside a handler does to the call.
            assert!(v.value != -1, "handler panic requested by the consumer");
            let value = v.value;
            async move {
                std::thread::sleep(Duration::from_millis(SLOW_MS));
                Total { value: value + 1 }
            }
        })
        .offer()
        .expect("offer");
    println!("PRODUCER offered");
    loop {
        std::thread::park();
    }
}

// ---------------------------------------------------------------- consumer

type Consumer = CalcConsumer<LolaRuntimeImpl>;

/// Finds the service and builds a consumer, or `None` when it is not found within `within`.
fn connect(rt: &LolaRuntimeImpl, within: Duration) -> Option<Consumer> {
    let deadline = Instant::now() + within;
    loop {
        match rt
            .find_service::<CalcInterface>(FindServiceSpecifier::Specific(specifier()))
            .get_available_instances()
        {
            Ok(found) => {
                if let Some(b) = found.into_iter().next() {
                    // The service can be found before its methods are ready: keep trying.
                    match b.build() {
                        Ok(c) => return Some(c),
                        Err(e) => println!("consumer build failed: {e:?}"),
                    }
                }
            },
            Err(e) => println!("discovery error: {e:?}"),
        }
        if Instant::now() > deadline {
            return None;
        }
        std::thread::sleep(Duration::from_millis(100));
    }
}

struct Checks(bool);

impl Checks {
    fn check(&mut self, what: &str, ok: bool, detail: String) {
        if ok {
            println!("OK {what}");
        } else {
            println!("FAIL {what}: {detail}");
            self.0 = false;
        }
    }

    fn finish(self) -> ExitCode {
        if self.0 {
            println!("RESULT: PASS");
            ExitCode::SUCCESS
        } else {
            println!("RESULT: FAIL");
            ExitCode::FAILURE
        }
    }
}

fn add(c: &Consumer, a: i32, b: i32) -> Result<i32, String> {
    block_on(c.add(Operands { a, b }))
        .map(|s| s.value)
        .map_err(|e| format!("{e:?}"))
}

fn scenario_basic(c: &Consumer, k: &mut Checks) {
    let t = Duration::from_secs(5);
    step("add", t);
    let r = add(c, 2, 40);
    k.check("method add(2,40) = 42", r == Ok(42), format!("{r:?}"));
    step("add negative", t);
    let r = add(c, -5, 3);
    k.check("method add(-5,3) = -2", r == Ok(-2), format!("{r:?}"));
    step("ping", t);
    let r = block_on(c.ping()).map(|s| s.value).map_err(|e| format!("{e:?}"));
    k.check("method with no arguments", r == Ok(4242), format!("{r:?}"));
    step("mix", t);
    let (x, y) = (wide(3), wide(200));
    let r = block_on(c.mix(x, y)).map(|s| *s).map_err(|e| format!("{e:?}"));
    k.check(
        "two arguments with mixed alignment",
        r.as_ref().ok() == Some(&mixed(x, y)),
        format!("{r:?} expected {:?}", mixed(x, y)),
    );
    step("reset (void method)", t);
    let r = block_on(c.reset(Total { value: 55 }))
        .map(|_| ())
        .map_err(|e| format!("{e:?}"));
    k.check("void method returns", r.is_ok(), format!("{r:?}"));
    step("last", t);
    let r = block_on(c.last()).map(|s| s.value).map_err(|e| format!("{e:?}"));
    k.check(
        "the void method's argument reached the producer",
        r == Ok(55),
        format!("{r:?}"),
    );
}

fn scenario_seq(c: &Consumer, k: &mut Checks) {
    step("2000 sequential calls", Duration::from_secs(60));
    let mut wrong = 0;
    let mut first_err = None;
    for i in 0..2000 {
        match add(c, i, i * 3) {
            Ok(v) if v == i * 4 => {},
            other => {
                wrong += 1;
                first_err.get_or_insert(format!("call {i}: {other:?}"));
            },
        }
    }
    k.check(
        "2000 sequential calls all right",
        wrong == 0,
        format!("{wrong} wrong, first {first_err:?}"),
    );
}

fn scenario_slow1(c: &Consumer, k: &mut Checks) {
    step("slow call", Duration::from_millis(SLOW_MS * 20));
    match block_on(c.slow(Total { value: 1 })) {
        Ok(s) => k.check("slow call", s.value == 2, format!("returned {}", s.value)),
        // Refused because the queue is full: a defined answer, not a failure.
        Err(e) => println!("DEFINED-ERROR slow call refused: {e:?}"),
    }
}

/// More concurrent calls on one consumer than the method's queueSize (2): each must end in the
/// right value or an error, none may hang or return a wrong value.
fn scenario_queue(c: &Consumer, k: &mut Checks) {
    step(
        "8 concurrent calls on one consumer",
        Duration::from_millis(SLOW_MS * 40),
    );
    let calls = (0..8).map(|i| c.slow(Total { value: 100 + i }));
    let results = block_on(futures::future::join_all(calls));
    let (mut served, mut refused, mut wrong) = (0, 0, Vec::new());
    for (i, r) in results.iter().enumerate() {
        match r {
            Ok(s) if s.value == 101 + i as i32 => served += 1,
            Ok(s) => wrong.push(format!("call {i} returned {}", s.value)),
            Err(e) => {
                refused += 1;
                println!("DEFINED-ERROR call {i}: {e:?}");
            },
        }
    }
    println!("INFO queue: {served} served, {refused} refused, {} wrong", wrong.len());
    k.check(
        "no wrong value under queue overflow",
        wrong.is_empty(),
        format!("{wrong:?}"),
    );
    k.check("at least one call served", served >= 1, format!("{served} served"));
}

/// Calls that must never return a wrong value, whatever else happens (used where the test
/// provokes a misconfiguration).
fn scenario_integrity(c: &Consumer, k: &mut Checks) {
    step("500 calls, integrity only", Duration::from_secs(60));
    let (mut ok, mut err, mut wrong) = (0, 0, Vec::new());
    for i in 0..500 {
        match add(c, i, 1000) {
            Ok(v) if v == i + 1000 => ok += 1,
            Ok(v) => wrong.push(format!("add({i},1000) = {v}")),
            Err(_) => err += 1,
        }
    }
    println!("INFO integrity: {ok} right, {err} errors, {} wrong", wrong.len());
    k.check(
        "no call returned a wrong value",
        wrong.is_empty(),
        format!("{:?}", wrong.iter().take(5).collect::<Vec<_>>()),
    );
}

/// The producer's handler panics on this call: the call must end (error), not hang.
fn scenario_handler_panic(c: &Consumer, k: &mut Checks) {
    step("call whose handler panics", Duration::from_secs(15));
    let r = block_on(c.slow(Total { value: -1 }))
        .map(|s| s.value)
        .map_err(|e| format!("{e:?}"));
    k.check(
        "a panicking handler gives the caller an error",
        r.is_err(),
        format!("{r:?}"),
    );
    step("call after the panic", Duration::from_secs(15));
    let r = add(c, 1, 2);
    println!("INFO call after the handler panic: {r:?}");
}

/// Waits until `file` exists (written by run_test.sh).
fn wait_for(file: &Path, limit: Duration) -> bool {
    let deadline = Instant::now() + limit;
    while !file.exists() {
        if Instant::now() > deadline {
            return false;
        }
        std::thread::sleep(Duration::from_millis(50));
    }
    true
}

fn scenario_crash(rt: &LolaRuntimeImpl, c: &Consumer, k: &mut Checks) {
    let dir = PathBuf::from(std::env::var("E2E_SYNC_DIR").unwrap_or_else(|_| ".".into()));
    step("before kill", Duration::from_secs(5));
    let r = add(c, 1, 1);
    k.check("call before the producer dies", r == Ok(2), format!("{r:?}"));
    println!("READY_FOR_KILL");
    let _ = std::fs::write(dir.join("ready"), b"");
    step("wait for kill", Duration::from_secs(30));
    if !wait_for(&dir.join("killed"), Duration::from_secs(25)) {
        k.check("producer killed by the driver", false, "no signal".into());
        return;
    }
    step("call after kill", Duration::from_secs(10));
    let r = add(c, 1, 1);
    k.check("call to a dead producer returns an error", r.is_err(), format!("{r:?}"));
    step("wait for restart", Duration::from_secs(30));
    if !wait_for(&dir.join("restarted"), Duration::from_secs(25)) {
        k.check("producer restarted by the driver", false, "no signal".into());
        return;
    }
    step("rediscover", Duration::from_secs(20));
    let Some(fresh) = connect(rt, Duration::from_secs(15)) else {
        k.check("rediscover the restarted producer", false, "not found".into());
        return;
    };
    println!("OK rediscover the restarted producer");
    step("call restarted producer", Duration::from_secs(5));
    let r = add(&fresh, 20, 22);
    k.check("call to the restarted producer", r == Ok(42), format!("{r:?}"));
    step("old consumer after restart", Duration::from_secs(10));
    let r = add(c, 1, 1);
    println!("INFO old consumer after restart: {r:?}");
}

fn consumer(config: &Path, scenario: &str) -> ExitCode {
    start_watchdog();
    let rt = runtime(config);
    let mut k = Checks(true);
    let find_within = if scenario == "early" {
        Duration::from_secs(30)
    } else {
        Duration::from_secs(10)
    };
    step("discovery", find_within + Duration::from_secs(5));
    let Some(c) = connect(&rt, find_within) else {
        k.check("discovery", false, "service not found".into());
        return k.finish();
    };
    match scenario {
        "basic" | "early" => scenario_basic(&c, &mut k),
        "seq" => scenario_seq(&c, &mut k),
        "slow1" => scenario_slow1(&c, &mut k),
        "queue" => scenario_queue(&c, &mut k),
        "integrity" => scenario_integrity(&c, &mut k),
        "handler_panic" => scenario_handler_panic(&c, &mut k),
        "crash" => scenario_crash(&rt, &c, &mut k),
        other => k.check("scenario", false, format!("unknown {other}")),
    }
    if let Ok(mut s) = STEP.lock() {
        *s = None;
    }
    k.finish()
}

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().collect();
    match (args.get(1).map(String::as_str), args.get(2)) {
        (Some("producer"), Some(config)) => producer(Path::new(config)),
        (Some("consumer"), Some(config)) => consumer(Path::new(config), args.get(3).map_or("basic", String::as_str)),
        _ => {
            eprintln!("usage: rust_method_lola_e2e producer <config> | consumer <config> <scenario>");
            ExitCode::FAILURE
        },
    }
}
