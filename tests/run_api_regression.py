"""Test actual library API using its real dictionary, mocked Arduino/I2S/NVS. No hardware."""
from datetime import datetime, timezone
from pathlib import Path
import json,os,subprocess,sys

def main():
 root=Path(__file__).resolve().parents[1];lib=root
 out=root/'.test-output'/datetime.now(timezone.utc).strftime('test_%Y%m%dT%H%M%S');shim=out/'shim';(shim/'driver').mkdir(parents=True)
 (shim/'Arduino.h').write_text('''#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <chrono>
#define PROGMEM
#define M_PI 3.14159265358979323846
template<class A,class B,class C> A constrain(A v,B a,C b){return v<a?a:v>b?b:v;}
inline uint32_t micros(){return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct SerialMock{template<class... T>void printf(const char*,T...){}};inline SerialMock Serial;
''')
 (shim/'esp_heap_caps.h').write_text('#pragma once\n#include <stdlib.h>\n#define MALLOC_CAP_SPIRAM 0\ninline void* heap_caps_malloc(size_t n,int){return malloc(n);}\n')
 (shim/'Preferences.h').write_text('''#pragma once
inline bool saved=false;inline uint32_t savedCrc=0;inline unsigned marks=0;
class Preferences{public:bool begin(const char*,bool){return true;}bool isKey(const char*){return saved;}
uint32_t getUInt(const char*,uint32_t){return savedCrc;}void end(){}size_t putUInt(const char*,uint32_t n){saved=true;savedCrc=n;marks++;return 4;}};
''')
 (shim/'driver/i2s.h').write_text('''#pragma once
#include <vector>
#include <stdint.h>
#include <stddef.h>
enum i2s_port_t{I2S_NUM_0,I2S_NUM_1,I2S_NUM_MAX};typedef int i2s_mode_t;
#define I2S_MODE_MASTER 1
#define I2S_MODE_TX 2
#define I2S_BITS_PER_SAMPLE_16BIT 16
#define I2S_BITS_PER_CHAN_16BIT 16
#define I2S_CHANNEL_FMT_RIGHT_LEFT 0
#define I2S_COMM_FORMAT_STAND_I2S 0
#define I2S_PIN_NO_CHANGE -1
#define ESP_OK 0
#define pdMS_TO_TICKS(n) (n)
struct i2s_config_t{int mode,sample_rate,bits_per_sample,bits_per_chan,channel_format,communication_format,dma_buf_count,dma_buf_len;bool tx_desc_auto_clear;};
struct i2s_pin_config_t{int mck_io_num,bck_io_num,ws_io_num,data_out_num,data_in_num;};
inline unsigned installedPorts=0,stereoErrors=0;inline bool failWrites=false;inline std::vector<int16_t> captured;
inline int i2s_driver_install(i2s_port_t,const i2s_config_t*,int,void*){installedPorts++;return 0;}
inline int i2s_driver_uninstall(i2s_port_t){installedPorts--;return 0;}
inline int i2s_set_pin(i2s_port_t,const i2s_pin_config_t*){return 0;}
inline int i2s_zero_dma_buffer(i2s_port_t){return 0;}
inline int i2s_write(i2s_port_t,const void* b,size_t bytes,size_t* written,int){
 if(failWrites){*written=0;return 1;}const int16_t* p=(const int16_t*)b;
 for(size_t i=0;i<bytes/2;i+=2){if(p[i]!=p[i+1])stereoErrors++;captured.push_back(p[i]);}*written=bytes;return 0;}
''')
 env={**os.environ,'ZIG_GLOBAL_CACHE_DIR':str(root/'.zig-global-cache'),'ZIG_LOCAL_CACHE_DIR':str(root/'.zig-cache')}
 command=[sys.executable,'-m','ziglang','c++','-std=c++17','-O2','-UNDEBUG','-DARDUINO','-DESP_PLATFORM','-I',str(shim),'-I',str(lib/'src')]
 command+=[str(lib/'src'/n) for n in ['MakoTalkEngine.cpp','MakoTalkImage.cpp','MakoTalkText.cpp','MakoTalkSpeaker.cpp']]
 command+=[str(lib/'tests/api_regression.cpp'),'-o',str(out/'test.exe')]
 p=subprocess.run(command,capture_output=True,env=env,timeout=120);(out/'build.log').write_bytes(p.stdout+p.stderr)
 if p.returncode:raise RuntimeError('Host compile failed: '+str(out/'build.log'))
 p=subprocess.run([str(out/'test.exe')],capture_output=True,timeout=40);(out/'execution.log').write_bytes(p.stdout+p.stderr)
 report=dict(passed=p.returncode==0,scope='real library API and real voice; mocked I2S/NVS, no hardware',assertions_enabled=True)
 (out/'report.json').write_text(json.dumps(report,indent=2));(out.parent/'latest_test.json').write_text(json.dumps(dict(run=str(out.relative_to(root)),**report),indent=2))
 print(p.stdout.decode(errors='replace'));print(json.dumps(dict(output=str(out),**report),indent=2))
 return 0 if report['passed'] else 1

if __name__=='__main__':raise SystemExit(main())
