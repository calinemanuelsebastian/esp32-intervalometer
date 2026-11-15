#!/usr/bin/env python3
"""Check the audited classic-ESP32 release path in a compiled ELF.

Usage: python3 verify_timer_iram.py /path/to/xtensa-esp32-elf-objdump firmware.elf
This checks placement and direct call targets. It does not measure IRQ latency.
"""
import re
import subprocess
import sys

if len(sys.argv)!=3:
    raise SystemExit(__doc__)
objdump,elf=sys.argv[1:]
symbols=subprocess.check_output([objdump,'-t','-C',elf],text=True)
dis=subprocess.check_output([objdump,'-d','-C',elf],text=True)
functions={}
for m in re.finditer(r'^([0-9a-f]+) <([^\n]+)>:\n(.*?)(?=^[0-9a-f]+ <|\Z)',dis,re.M|re.S):
    functions[m[2]]=(int(m[1],16),m[3])
roots=('PulseTimer::alarm(gptimer_t*, gptimer_alarm_event_data_t const*, void*)',
       'PulseTimer::setPins(bool, bool)',
       'PulseTimer::commitStart(unsigned long long, unsigned long long, bool)','gptimer_default_isr',
       'esp_timer_get_time','esp_timer_impl_get_counter_reg',
       'timer_hal_capture_and_get_counter_value',
       'xPortEnterCriticalTimeout','vPortExitCritical','esp_cpu_compare_and_set')
# Assertion/panic branches are not normal timing paths. Their diagnostic strings
# may reside in flash. An assertion failure is a fault, not a supported exposure.
excluded={'__assert_func'}
def instructions(text):
    result={}
    for match in re.finditer(r'^\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)\s*([^\n]*)',text,re.M):
        result[int(match[1],16)]=(len(match[2])//2,match[3],match[4])
    return result

# Follow instruction boundaries from entry and branch targets. Linear objdump
# sometimes decodes one byte of Xtensa alignment padding as an instruction and
# then invents calls across the following assertion/return block. Re-decode a
# missing branch target from that exact address; never whitelist invented calls.
def reachable(name):
    start,body=functions[name]
    end=min((address for address,_ in functions.values() if address>start),default=start+65536)
    decoded=instructions(body);todo=[start];visited=set()
    while todo:
        pc=todo.pop()
        if pc in visited:continue
        visited.add(pc)
        if pc not in decoded:
            chunk=subprocess.check_output([objdump,'-d','-C','--start-address='+hex(pc),
                                           '--stop-address='+hex(end),elf],text=True)
            decoded.update(instructions(chunk))
        assert pc in decoded,('Cannot decode branch target',name,hex(pc))
        length,op,args=decoded[pc]
        assert not op.startswith('.'),('Data reached as code',name,hex(pc),op)
        yield op,args
        target=re.search(r'([0-9a-f]+) <([^>]+)>',args)
        if op.startswith('ret') or op in ('break','break.n','ill','ill.n'):continue
        if op.startswith('call') and target and target[2] in excluded:continue
        if op=='j' or op.startswith('b'):
            assert target,('Unresolved branch',name,op,args)
            address=int(target[1],16)
            if start<=address<end:todo.append(address)
            elif op=='j':yield 'call8',args # tail call
            else:raise AssertionError(('Branch outside function',name,args))
            if op=='j':continue
        if pc+length<end:todo.append(pc+length)

seen=set();pending=list(roots)
while pending:
    name=pending.pop()
    if name in seen or name in excluded:continue
    seen.add(name)
    if name not in functions:raise AssertionError('Missing function: '+name)
    address,body=functions[name]
    assert 0x40080000<=address<0x400a0000,(name,hex(address))
    symbol_lines=[line for line in symbols.splitlines() if line.endswith(' '+name)]
    assert symbol_lines and any('.iram0.text' in line for line in symbol_lines),name
    for op,args in reachable(name):
        if op.startswith('callx'):
            # Driver dispatches our registered callback. Lock helpers restore
            # interrupt level via _xtos_set_intlevel in ROM (manually audited).
            assert name in ('gptimer_default_isr','xPortEnterCriticalTimeout','vPortExitCritical'),name
            if name!='gptimer_default_isr':assert '<_xtos_set_intlevel>' in body,name
        elif op.startswith('call'):
            match=re.search(r'([0-9a-f]+) <([^>]+)>',args)
            assert match,('Unresolved call',name,args)
            target,callee=match.groups()
            if callee in excluded:continue
            address=int(target,16)
            if 0x40000000<=address<0x40070000:continue # ROM
            assert 0x40080000<=address<0x400a0000,(name,callee,hex(address))
            pending.append(callee)
for name in sorted(seen):print('IRAM: '+name)
for name in roots[:3]:
    assert not re.search(r'\(3f4[0-9a-f]+ <',functions[name][1]),'Flash data literal: '+name
assert any('.dram0.bss' in line and line.endswith(' pulse') for line in symbols.splitlines()),'PulseTimer context not in DRAM'
assert any('esp_intr_set_in_iram' in line for line in symbols.splitlines()),'Missing interrupt configuration API'
assert any('gptimer_get_intr_handle' in line for line in symbols.splitlines()),'Missing interrupt handle accessor'
print('PASS: audited normal release call chain uses IRAM/ROM; context is in DRAM.')
print('Indirect sites checked against pinned driver and FreeRTOS implementation.')
print('Assertion/panic paths excluded; this is not a physical timing measurement.')
