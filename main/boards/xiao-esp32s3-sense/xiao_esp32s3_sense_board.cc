#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "esp32_camera.h"
#include "mcp_server.h"
#include "settings.h"

#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <driver/spi_common.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <ctime>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <dirent.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <vector>
#include <esp_http_client.h>
#include <esp_crt_bundle.h>
#include <sys/stat.h>

#define TAG "XiaoEsp32s3SenseBoard"
#define STREAM_BOUNDARY "xiao_mjpeg_frame"

// Embedded HTML gallery page
static const char GALLERY_HTML[] =
"<!DOCTYPE html><html><head><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
"<title>XIAO Photos</title><style>"
"body{margin:0;background:#111;color:#eee;font-family:sans-serif}"
"h1{text-align:center;padding:15px;color:#4fc3f7;margin:0}"
".toolbar{display:flex;gap:10px;justify-content:center;flex-wrap:wrap;padding:0 12px 12px}"
"#info{text-align:center;padding:8px;color:#aaa;font-size:14px}"
".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:8px;padding:10px}"
".card{background:#222;border-radius:6px;overflow:hidden;cursor:pointer;transition:transform .2s}"
".card:hover{transform:scale(1.04)}"
".card img{width:100%;height:130px;object-fit:cover;display:block}"
".name{font-size:11px;padding:4px 6px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
".lb{display:none;position:fixed;inset:0;background:rgba(0,0,0,.7);z-index:9;align-items:center;justify-content:center;flex-direction:column;padding:20px}"
".lb.on{display:flex}"
".lb-modal{background:#1a1a2e;border:1px solid #4fc3f7;border-radius:8px;padding:16px;max-width:85vw;max-height:90vh;overflow:auto;display:flex;flex-direction:column;align-items:center;position:relative}"
".lb img{max-width:70vw;max-height:60vh;object-fit:contain;border-radius:6px}"
"#lb-video{max-width:70vw;max-height:60vh;border-radius:6px;display:none}"
".x{position:absolute;top:8px;right:12px;font-size:28px;cursor:pointer;color:#fff;background:none;border:none}"
".fn{margin-top:8px;color:#aaa;font-size:13px}"
".lb-acts{display:flex;gap:8px;flex-wrap:wrap;justify-content:center;margin-top:12px}"
".vid-icon{width:100%;height:130px;display:flex;align-items:center;justify-content:center;font-size:44px;background:#0d0d1a}"
".btn{padding:8px 20px;background:#4fc3f7;color:#111;border:none;border-radius:6px;cursor:pointer;font-size:14px}"
".btn.alt{background:#f6c344}"
".btn[disabled]{background:#555;color:#bbb;cursor:not-allowed}"
".tg-panel{background:#1a1a2e;border:1px solid #4fc3f7;border-radius:8px;padding:14px;margin:0 12px 12px;max-width:620px;margin-left:auto;margin-right:auto}"
".tg-panel h3{margin:0 0 10px;color:#4fc3f7;font-size:15px;display:flex;align-items:center;gap:6px;cursor:pointer}"
".tg-body{display:none}"
".tg-body.open{display:block}"
".tg-row{display:flex;align-items:center;gap:8px;margin-bottom:8px;flex-wrap:wrap}"
".tg-row label{color:#aaa;font-size:13px;width:85px;flex-shrink:0}"
".tg-inp{flex:1;min-width:180px;padding:6px 8px;background:#2a2a3e;border:1px solid #555;border-radius:4px;color:#eee;font-size:13px}"
"#tg-status{margin-top:6px;font-size:13px;color:#aaa;min-height:18px}"
"</style></head><body>"
"<h1>&#128247; XIAO Camera</h1>"
"<div class=toolbar>"
"<button class=btn onclick='capturePhoto()'>Chup anh</button>"
"<button class=btn alt onclick='loadFiles()'>Refresh</button>"
"<button id=sb class=btn onclick='toggleStream()' style='background:#4caf50;color:#fff'>&#9654; Live</button>"
"<button id=rb class=btn onclick='toggleRecord()' style='background:#e53935;color:#fff'>&#9679; Quay video</button>"
"<button id=rot-btn class=btn onclick='rotateCamera()' style='background:#9c27b0;color:#fff'>&#128260; Xoay camera</button>"
"</div>"
"<div id=sv style='display:none;text-align:center;padding:10px 10px 0'>"
"<img id=si style='max-width:100%;max-height:70vh;border-radius:6px;display:block;margin:0 auto'>"
"<br><button class=btn style='margin-top:8px;background:#c62828;color:#fff' onclick='stopStream()'>&#9209; Dung stream</button>"
"</div>"
"<div class=tg-panel>"
"<h3 onclick=\"document.getElementById('tg-body').classList.toggle('open')\">&#128229; Telegram &#9660;</h3>"
"<div class=tg-body id=tg-body>"
"<div class=tg-row><label>Bot Token:</label><input id=tg-token class=tg-inp type=password placeholder='123456789:ABCDEF...'></div>"
"<div class=tg-row><label>Chat ID:</label><input id=tg-chatid class=tg-inp type=text placeholder='1234567890'></div>"
"<div class=tg-row>"
"<button class=btn onclick='saveTgConfig()'>Luu cau hinh</button>"
"<button class=btn alt id=tg-send onclick='sendTelegram()'>&#128247; Gui anh</button>"
"<button class=btn id=tg-vid onclick='sendTelegramVideo()' style='background:#ff7043;color:#fff'>&#127909; Gui video</button>"
"</div>"
"<div id=tg-status></div>"
"</div>"
"</div>"
"<div id=info>Loading&#8230;</div>"
"<div class=grid id=g></div>"
"<div class=lb id=lb>"
"<div class=lb-modal>"
"<button class=x onclick='closeLb()'>\u2715</button>"
"<img id=lbimg src='' style='display:none'>"
"<video id=lb-video controls style='display:none'></video>"
"<div class=fn id=lbname></div>"
"<div class=lb-acts>"
"<button class=btn onclick='sendLbTelegram()' style='background:#229ed9;color:#fff'>&#128228; Gui Telegram</button>"
"<a id=lb-dl class=btn style='background:#555;color:#fff;text-decoration:none' download>&#8681; Tai xuong</a>"
"<button class=btn id=lb-del onclick='deleteLbFile()' style='background:#d32f2f;color:#fff'>&#128465; Xoa file</button>"
"</div>"
"</div>"
"</div>"
"<script>"
"let busy=false,streaming=false,recording=false,recTimer=null,recSecs=0,lbFile='';"
"function stopStream(){streaming=false;document.getElementById('sv').style.display='none';document.getElementById('si').src='';document.getElementById('sb').textContent='\u25b6 Live';}"
"function toggleStream(){if(!streaming){streaming=true;document.getElementById('sv').style.display='block';document.getElementById('si').src='http://'+location.hostname+':81/stream?t='+Date.now();document.getElementById('sb').textContent='\u23f9 Stop';}else stopStream();}"
"function viewPhoto(n){lbFile=n;var v=document.getElementById('lb-video');v.pause();v.src='';v.style.display='none';document.getElementById('lbimg').style.display='block';document.getElementById('lbimg').src='/photo/'+n;document.getElementById('lbname').textContent=n;var a=document.getElementById('lb-dl');a.href='/photo/'+n;a.download=n;document.getElementById('lb').classList.add('on');}"
"function viewVideo(n){lbFile=n;document.getElementById('lbimg').style.display='none';var vid=document.getElementById('lb-video');vid.style.display='block';vid.src='/photo/'+n;vid.load();document.getElementById('lbname').textContent=n;var a=document.getElementById('lb-dl');a.href='/photo/'+n;a.download=n;document.getElementById('lb').classList.add('on');}"
"function closeLb(){var v=document.getElementById('lb-video');v.pause();v.src='';v.style.display='none';document.getElementById('lbimg').style.display='none';document.getElementById('lb').classList.remove('on');lbFile='';}"
"function deleteLbFile(){if(!lbFile)return;if(!confirm('Xoa file \"'+lbFile+'\"?'))return;var btn=document.getElementById('lb-del');btn.disabled=true;fetch('/api/files?filename='+encodeURIComponent(lbFile),{method:'DELETE'}).then(r=>r.json()).then(d=>{if(d.ok){closeLb();loadFiles();}else alert('Loi: '+(d.message||'unknown'));}).catch(()=>{alert('Loi ket noi');}).finally(()=>{btn.disabled=false;});}"
"function sendLbTelegram(){if(!lbFile)return;var s=document.getElementById('tg-status');s.style.color='#aaa';document.getElementById('tg-body').classList.add('open');var isVid=lbFile.slice(-4)==='.avi';s.textContent=(isVid?'Dang gui video...':'Dang gui anh...')+' ('+lbFile+')';fetch('/api/telegram/sendfile',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({filename:lbFile})}).then(r=>r.json()).then(d=>{if(d.ok){s.style.color='#4caf50';s.textContent='Da gui: '+lbFile;}else{s.style.color='#f44336';s.textContent='Loi: '+(d.message||'unknown');}}).catch(()=>{s.style.color='#f44336';s.textContent='Loi ket noi';});}"
"function setInfo(t){document.getElementById('info').textContent=t;}"
"function toggleRecord(){"
"var rb=document.getElementById('rb');"
"if(recording){"
"recording=false;clearInterval(recTimer);"
"rb.textContent='\u23f9 Dung...';rb.disabled=true;"
"setInfo('Dang luu video...');"
"fetch('/api/record/stop',{method:'POST'}).then(r=>r.json()).then(d=>{"
"rb.disabled=false;rb.textContent='\u25cf Quay video';rb.style.background='#e53935';"
"if(d.ok){setInfo('Da luu: '+d.filename);loadFiles();}else setInfo('Loi: '+(d.message||'unknown'));"
"}).catch(()=>{rb.disabled=false;rb.textContent='\u25cf Quay video';setInfo('Loi dung quay');});"
"}else{"
"setInfo('Bat dau quay...');"
"fetch('/api/record/start',{method:'POST'}).then(r=>r.json()).then(d=>{"
"if(!d.ok){setInfo('Loi: '+(d.message||''));return;}"
"recording=true;recSecs=0;"
"rb.textContent='\u23f9 Dung (0s)';rb.style.background='#b71c1c';"
"recTimer=setInterval(()=>{recSecs++;rb.textContent='\u23f9 Dung ('+recSecs+'s)';},1000);"
"}).catch(()=>setInfo('Loi bat dau quay'));"
"}}"
"function capturePhoto(){if(busy)return;busy=true;setInfo('Dang chup anh...');fetch('/api/capture',{method:'POST'}).then(r=>r.json()).then(data=>{if(!data.ok)throw new Error(data.message||'Capture failed');setInfo('Da luu: '+data.filename);return loadFiles();}).catch(err=>{setInfo(err.message||'Chup anh that bai');}).finally(()=>{busy=false;});}"
"function saveTgConfig(){"
"var token=document.getElementById('tg-token').value.trim();"
"var chatId=document.getElementById('tg-chatid').value.trim();"
"if(!token&&!chatId){document.getElementById('tg-status').textContent='Nhap Bot Token va Chat ID';return;}"
"var body={};"
"if(token)body.token=token;"
"if(chatId)body.chat_id=chatId;"
"fetch('/api/telegram/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})"
".then(r=>r.json()).then(d=>{"
"var s=document.getElementById('tg-status');"
"if(d.ok){s.style.color='#4caf50';s.textContent='Da luu cau hinh Telegram!';document.getElementById('tg-token').value='';}else{s.style.color='#f44336';s.textContent='Loi: '+(d.message||'unknown');}"
"}).catch(()=>{document.getElementById('tg-status').textContent='Loi ket noi';});}"
"function sendTelegram(){"
"var btn=document.getElementById('tg-send');btn.disabled=true;"
"var s=document.getElementById('tg-status');s.style.color='#aaa';s.textContent='Dang chup va gui...';"
"fetch('/api/telegram/send',{method:'POST'})"
".then(r=>r.json()).then(d=>{"
"if(d.ok){s.style.color='#4caf50';s.textContent='Da gui anh len Telegram!';}else{s.style.color='#f44336';s.textContent='Loi: '+(d.message||'unknown');}"
"}).catch(()=>{s.style.color='#f44336';s.textContent='Loi ket noi';})"
".finally(()=>{btn.disabled=false;});}"
"function loadTgConfig(){"
"fetch('/api/telegram/config').then(r=>r.json()).then(d=>{"
"if(d.chat_id)document.getElementById('tg-chatid').value=d.chat_id;"
"if(d.has_token){var t=document.getElementById('tg-token');t.placeholder='(da luu - nhap lai de thay doi)';}"
"}).catch(()=>{});}"
"loadTgConfig();"
"function rotateCamera(){"
"fetch('/api/camera/rotate',{method:'POST'})"
".then(r=>r.json()).then(d=>{"
"if(d.ok)document.getElementById('rot-btn').textContent='\u21ba '+d.name;"
"}).catch(()=>{}); }"
"function sendTelegramVideo(){"
"var btn=document.getElementById('tg-vid');btn.disabled=true;"
"var s=document.getElementById('tg-status');s.style.color='#aaa';s.textContent='Dang gui video (co the mat 1-2 phut)...';"
"fetch('/api/telegram/sendvideo',{method:'POST'})"
".then(r=>r.json()).then(d=>{"
"if(d.ok){s.style.color='#4caf50';s.textContent=d.message||'Da gui video!';}else{s.style.color='#f44336';s.textContent='Loi: '+(d.message||'unknown');}"
"}).catch(()=>{s.style.color='#f44336';s.textContent='Loi ket noi';})"
".finally(()=>{btn.disabled=false;});}"
"function loadFiles(){"
"setInfo('Loading&#8230;');"
"document.getElementById('g').innerHTML='';"
"fetch('/api/files').then(r=>r.json()).then(files=>{"
"const info=document.getElementById('info');const g=document.getElementById('g');"
"if(!files.length){info.textContent='Chua co file nao. Chup anh hoac quay video!';return;}"
"files.sort((a,b)=>b.name.localeCompare(a.name));"
"info.textContent=files.length+' file(s) \u2014 tap de xem';"
"files.forEach(f=>{"
"const d=document.createElement('div');d.className='card';"
"if(f.type==='video'){d.onclick=()=>viewVideo(f.name);d.innerHTML='<div class=vid-icon>&#127909;</div><div class=name>'+f.name+'</div>';}"
"else{d.onclick=()=>viewPhoto(f.name);d.innerHTML='<img src=/photo/'+f.name+' loading=lazy><div class=name>'+f.name+'</div>';}"
"g.appendChild(d);"
"});}).catch(()=>{document.getElementById('info').textContent='Failed to load files';});}"
"loadFiles();"
"</script></body></html>";


class XiaoEsp32s3SenseBoard : public WifiBoard {
private:
    Button boot_button_;
    Esp32Camera* camera_ = nullptr;
    NoAudioCodecSimplexPdm* pdm_codec_ = nullptr;
    bool sd_mounted_ = false;
    httpd_handle_t server_ = nullptr;
    httpd_handle_t stream_server_ = nullptr;

    // ---- Telegram config ----
    std::string tg_token_;
    std::string tg_chat_id_;

    // ---- Camera rotation state (0=Normal,1=Mirror,2=180°,3=VFlip) ----
    int camera_rotation_ = 0;

    // ---- Get a real-time (fresh) frame by flushing stale buffered frames ----
    // With fb_count=2 + CAMERA_GRAB_WHEN_EMPTY, driver stops filling buffers once
    // they are full. Old frames stay in queue indefinitely. We must return all
    // queued frames to the driver, let it re-capture, then grab once more.
    camera_fb_t* GetFreshCameraFrame() {
        // Flush fb_count(=2) stale frames from queue
        static constexpr int FB_COUNT = 2;
        for (int i = 0; i < FB_COUNT; i++) {
            camera_fb_t* stale = esp_camera_fb_get();
            if (stale) esp_camera_fb_return(stale);
        }
        // Wait one full frame period (10fps worst case = 100ms) for sensor to expose
        vTaskDelay(pdMS_TO_TICKS(120));
        // Now grab the fresh frame
        camera_fb_t* fb = nullptr;
        for (int retry = 0; retry < 5; retry++) {
            fb = esp_camera_fb_get();
            if (fb && fb->len > 4 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8) break;
            if (fb) { esp_camera_fb_return(fb); fb = nullptr; }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        return fb; // caller must call esp_camera_fb_return()
    }

    // ---- Video recording state ----
    volatile bool recording_ = false;
    TaskHandle_t record_task_ = nullptr;
    std::string last_video_name_;

    // Audio ring buffer (allocated in PSRAM during recording)
    static constexpr size_t REC_RING_SAMPLES = 48000; // 3s at 16kHz
    int16_t* rec_ring_ = nullptr;
    volatile size_t rec_ring_wr_ = 0;
    volatile size_t rec_ring_rd_ = 0;
    portMUX_TYPE rec_ring_mux_ = portMUX_INITIALIZER_UNLOCKED;
    int32_t rec_dc_est_ = 0;  // Running DC estimate for HP filter (removes PDM mic bias)

    // ---- AVI write helpers ----
    static void AviWu32(FILE* f, uint32_t v) { fwrite(&v, 4, 1, f); }
    static void AviWu16(FILE* f, uint16_t v) { fwrite(&v, 2, 1, f); }
    static void AviWcc(FILE* f, const char* s) { fwrite(s, 1, 4, f); }
    static long AviOpenChunk(FILE* f, const char* tag) {
        AviWcc(f, tag);
        long pos = ftell(f);
        AviWu32(f, 0);
        return pos;
    }
    static void AviCloseChunk(FILE* f, long pos) {
        long end = ftell(f);
        uint32_t sz = (uint32_t)(end - pos - 4);
        fseek(f, pos, SEEK_SET);
        AviWu32(f, sz);
        fseek(f, end, SEEK_SET);
        if (sz & 1) fputc(0, f); // word-align padding
    }
    static long AviOpenList(FILE* f, const char* list_fourcc) {
        fwrite("LIST", 1, 4, f);
        long pos = ftell(f);
        AviWu32(f, 0);
        AviWcc(f, list_fourcc);
        return pos;
    }

    // ---- Recording task ----
    static void RecordingTaskFn(void* arg) {
        static_cast<XiaoEsp32s3SenseBoard*>(arg)->DoRecording();
        vTaskDelete(nullptr);
    }

    void DoRecording() {
        // Allocate ring buffer
        rec_ring_ = new int16_t[REC_RING_SAMPLES];
        if (!rec_ring_) {
            ESP_LOGE(TAG, "Failed to allocate audio ring buffer");
            recording_ = false;
            record_task_ = nullptr;
            return;
        }
        rec_ring_wr_ = 0;
        rec_ring_rd_ = 0;

        // Install audio tap callback
        // Pre-warm DC estimate: on the first callback, run faster alpha to converge
        // in ~4ms instead of waiting 80ms. rec_dc_warmup_ counts how many samples
        // have been processed; during the first 512 samples use >>4 (fast), then >>6.
        rec_dc_est_ = 0;
        if (pdm_codec_) {
            pdm_codec_->SetAudioTapCallback([this](const int16_t* buf, int n) {
                portENTER_CRITICAL(&rec_ring_mux_);
                for (int i = 0; i < n; i++) {
                    // Two-stage DC blocking HP filter:
                    // - First 512 samples (32ms): α=15/16 (fast convergence to remove +2700 bias)
                    // - After that: α=63/64 (~40 Hz cutoff, stable tracking)
                    // Both still pass all speech frequencies (>100 Hz).
                    bool warmup = (rec_ring_wr_ < 512);
                    rec_dc_est_ += ((int32_t)buf[i] - rec_dc_est_) >> (warmup ? 4 : 6);
                    int16_t sample = (int16_t)((int32_t)buf[i] - rec_dc_est_);
                    size_t next_wr = (rec_ring_wr_ + 1) % REC_RING_SAMPLES;
                    if (next_wr != rec_ring_rd_) {
                        rec_ring_[rec_ring_wr_] = sample;
                        rec_ring_wr_ = next_wr;
                    }
                    // if full, drop newest sample
                }
                portEXIT_CRITICAL(&rec_ring_mux_);
            });
        }

        // Build filename
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        char filepath[72];
        snprintf(filepath, sizeof(filepath),
            SD_MOUNT_POINT "/vid_%04d%02d%02d_%02d%02d%02d.avi",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);

        FILE* f = fopen(filepath, "wb");
        if (!f) {
            ESP_LOGE(TAG, "Cannot open video file: %s (errno=%d)", filepath, errno);
            if (pdm_codec_) pdm_codec_->SetAudioTapCallback(nullptr);
            delete[] rec_ring_; rec_ring_ = nullptr;
            recording_ = false;
            record_task_ = nullptr;
            return;
        }

        // AVI recording parameters
        static constexpr int VIDEO_FPS = 10;
        static constexpr int AUDIO_RATE = AUDIO_INPUT_SAMPLE_RATE; // 16000
        static constexpr int AUDIO_PER_FRAME = AUDIO_RATE / VIDEO_FPS; // 1600 samples/frame
        static constexpr int VID_W = 640, VID_H = 480;

        // --- Positions to update at finalization ---
        long riff_size_pos, movi_size_pos, avih_frames_pos, vstrh_len_pos, astrh_len_pos;
        long movi_data_pos;
        uint32_t total_frames = 0;
        uint32_t total_audio_samples = 0;
        std::vector<uint8_t> idx1;
        idx1.reserve(3600 * 16); // reserve for ~3 minutes

        // ---- Write AVI file header ----
        fwrite("RIFF", 1, 4, f);
        riff_size_pos = ftell(f);
        AviWu32(f, 0);
        fwrite("AVI ", 1, 4, f);

        long hdrl_pos = AviOpenList(f, "hdrl");

        // avih
        {
            long avih_pos = AviOpenChunk(f, "avih");
            AviWu32(f, 1000000 / VIDEO_FPS); // MicroSecPerFrame
            AviWu32(f, 0);                   // MaxBytesPerSec
            AviWu32(f, 0);                   // PaddingGranularity
            AviWu32(f, 0x0910);              // Flags: AVIF_HASINDEX|AVIF_ISINTERLEAVED
            avih_frames_pos = ftell(f);
            AviWu32(f, 0);                   // TotalFrames (update later)
            AviWu32(f, 0);                   // InitialFrames
            AviWu32(f, 2);                   // Streams
            AviWu32(f, 0);                   // SuggestedBufferSize
            AviWu32(f, VID_W);
            AviWu32(f, VID_H);
            AviWu32(f, 0); AviWu32(f, 0); AviWu32(f, 0); AviWu32(f, 0); // Reserved
            AviCloseChunk(f, avih_pos);
        }

        // Video stream strl
        {
            long vstrl_pos = AviOpenList(f, "strl");
            long vstrh_pos = AviOpenChunk(f, "strh");
            fwrite("vids", 1, 4, f);
            fwrite("MJPG", 1, 4, f);
            AviWu32(f, 0);           // Flags
            AviWu16(f, 0);           // Priority
            AviWu16(f, 0);           // Language
            AviWu32(f, 0);           // InitialFrames
            AviWu32(f, 1);           // Scale
            AviWu32(f, VIDEO_FPS);   // Rate
            AviWu32(f, 0);           // Start
            vstrh_len_pos = ftell(f);
            AviWu32(f, 0);           // Length (update later)
            AviWu32(f, 0);           // SuggestedBufferSize
            AviWu32(f, 0xFFFFFFFF);  // Quality
            AviWu32(f, 0);           // SampleSize
            AviWu16(f, 0); AviWu16(f, 0); AviWu16(f, VID_W); AviWu16(f, VID_H);
            AviCloseChunk(f, vstrh_pos);

            long vstrf_pos = AviOpenChunk(f, "strf");
            AviWu32(f, 40);          // biSize (BITMAPINFOHEADER)
            AviWu32(f, VID_W);
            AviWu32(f, VID_H);
            AviWu16(f, 1);           // biPlanes
            AviWu16(f, 24);          // biBitCount
            fwrite("MJPG", 1, 4, f); // biCompression
            AviWu32(f, 0); AviWu32(f, 0); AviWu32(f, 0); AviWu32(f, 0); AviWu32(f, 0);
            AviCloseChunk(f, vstrf_pos);

            AviCloseChunk(f, vstrl_pos);
        }

        // Audio stream strl
        {
            long astrl_pos = AviOpenList(f, "strl");
            long astrh_pos = AviOpenChunk(f, "strh");
            fwrite("auds", 1, 4, f);
            AviWu32(f, 0x00000001);  // fccHandler = PCM
            AviWu32(f, 0);           // Flags
            AviWu16(f, 0); AviWu16(f, 0); // Priority, Language
            AviWu32(f, 0);           // InitialFrames
            AviWu32(f, 1);           // Scale
            AviWu32(f, AUDIO_RATE);  // Rate (samples/sec)
            AviWu32(f, 0);           // Start
            astrh_len_pos = ftell(f);
            AviWu32(f, 0);           // Length (update later, in samples)
            AviWu32(f, 0);           // SuggestedBufferSize
            AviWu32(f, 0xFFFFFFFF);  // Quality
            AviWu32(f, 2);           // SampleSize (2 bytes)
            AviWu16(f, 0); AviWu16(f, 0); AviWu16(f, 0); AviWu16(f, 0);
            AviCloseChunk(f, astrh_pos);

            long astrf_pos = AviOpenChunk(f, "strf");
            AviWu16(f, 1);                  // wFormatTag = PCM
            AviWu16(f, 1);                  // nChannels = mono
            AviWu32(f, AUDIO_RATE);         // nSamplesPerSec
            AviWu32(f, AUDIO_RATE * 2);     // nAvgBytesPerSec
            AviWu16(f, 2);                  // nBlockAlign
            AviWu16(f, 16);                 // wBitsPerSample
            AviWu16(f, 0);                  // cbSize
            AviCloseChunk(f, astrf_pos);

            AviCloseChunk(f, astrl_pos);
        }

        AviCloseChunk(f, hdrl_pos);

        // Open movi LIST
        movi_size_pos = AviOpenList(f, "movi");
        movi_data_pos = ftell(f); // first data byte inside movi

        // Audio sample buffer (heap-allocated)
        std::vector<int16_t> audio_buf(AUDIO_PER_FRAME);

        int64_t frame_interval_ms = 1000 / VIDEO_FPS;
        int64_t rec_start_ms = esp_timer_get_time() / 1000;
        static constexpr int64_t MAX_REC_MS = 180000; // 3 minutes

        // Pre-fill: wait until ring buffer has at least 1 full frame worth of audio.
        // This prevents frame-0 underrun (ring buffer is empty right after tap installed).
        {
            int64_t prefill_deadline = esp_timer_get_time() / 1000 + frame_interval_ms * 3;
            while (recording_) {
                portENTER_CRITICAL(&rec_ring_mux_);
                size_t wr = rec_ring_wr_, rd = rec_ring_rd_;
                int avail = (int)((wr >= rd) ? (wr - rd) : (REC_RING_SAMPLES - rd + wr));
                portEXIT_CRITICAL(&rec_ring_mux_);
                if (avail >= AUDIO_PER_FRAME) break;
                if (esp_timer_get_time() / 1000 >= prefill_deadline) break;
                vTaskDelay(pdMS_TO_TICKS(5));
            }
        }

        // ===== RECORDING LOOP =====
        while (recording_) {
            // Auto-stop after max recording time
            if ((esp_timer_get_time() / 1000 - rec_start_ms) > MAX_REC_MS) {
                ESP_LOGW(TAG, "Max recording time reached, stopping");
                break;
            }

            int64_t frame_start = esp_timer_get_time() / 1000;

            // Get camera frame
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }
            if (fb->format != PIXFORMAT_JPEG || fb->len < 4 ||
                fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
                esp_camera_fb_return(fb);
                vTaskDelay(pdMS_TO_TICKS(10));
                continue;
            }

            uint32_t fb_len = (uint32_t)fb->len;

            // ---- Write video chunk "00dc" ----
            uint32_t vid_offset = (uint32_t)(ftell(f) - movi_data_pos);
            fwrite("00dc", 1, 4, f);
            AviWu32(f, fb_len);
            fwrite(fb->buf, 1, fb_len, f);
            if (fb_len & 1) fputc(0, f);
            esp_camera_fb_return(fb);

            // Add video index entry
            {
                size_t n = idx1.size();
                idx1.resize(n + 16);
                uint8_t* ep = idx1.data() + n;
                uint32_t vflags = 0x10; // AVIIF_KEYFRAME
                memcpy(ep,    "00dc", 4);
                memcpy(ep+4,  &vflags,     4);
                memcpy(ep+8,  &vid_offset, 4);
                memcpy(ep+12, &fb_len,     4);
            }
            total_frames++;

            // ---- Collect audio for this frame ----
            int wanted = AUDIO_PER_FRAME;
            int got = 0;
            // Deadline measured from NOW (after camera+SD write), not from frame_start.
            // Camera fb_get + SD write takes ~100-150ms, so frame_start+100ms is already
            // in the past. Give up to 2x frame interval (200ms) from audio collection start.
            int64_t audio_deadline = esp_timer_get_time() / 1000 + frame_interval_ms * 2;

            while (got < wanted) {
                int avail = 0;
                portENTER_CRITICAL(&rec_ring_mux_);
                size_t wr = rec_ring_wr_, rd = rec_ring_rd_;
                avail = (int)((wr >= rd) ? (wr - rd) : (REC_RING_SAMPLES - rd + wr));
                portEXIT_CRITICAL(&rec_ring_mux_);

                if (avail > 0) {
                    int to_read = (avail < (wanted - got)) ? avail : (wanted - got);
                    portENTER_CRITICAL(&rec_ring_mux_);
                    for (int i = 0; i < to_read; i++) {
                        audio_buf[got + i] = rec_ring_[rec_ring_rd_];
                        rec_ring_rd_ = (rec_ring_rd_ + 1) % REC_RING_SAMPLES;
                    }
                    portEXIT_CRITICAL(&rec_ring_mux_);
                    got += to_read;
                }

                if (got < wanted) {
                    if (esp_timer_get_time() / 1000 >= audio_deadline) break;
                    // Wait for AudioInputTask to fill the ring buffer via tap callback.
                    // Do NOT call pdm_codec_->Read() here - it would race with AudioInputTask
                    // which also calls Read() on the same I2S channel (undefined behavior).
                    vTaskDelay(pdMS_TO_TICKS(5));
                }
            }
            // Pad with last sample to avoid DC-jump click if ring buffer ran dry.
            // DC filter already removes bias so samples are near 0, but mid-speech
            // values are non-zero — repeating last value is smoother than forcing 0.
            {
                int16_t pad_val = (got > 0) ? audio_buf[got - 1] : 0;
                for (int i = got; i < wanted; i++) audio_buf[i] = pad_val;
            }

            // Moderate software gain: PDM mic raw is low amplitude.
            // 2x (6 dB) raises voice to audible levels without clipping normal speech.
            // Higher gain amplifies noise equally — 8x was too aggressive.
            static constexpr int REC_AUDIO_GAIN = 2;
            for (int i = 0; i < wanted; i++) {
                int32_t v = (int32_t)audio_buf[i] * REC_AUDIO_GAIN;
                audio_buf[i] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
            }

            uint32_t audio_bytes = (uint32_t)(wanted * sizeof(int16_t));

            // ---- Write audio chunk "01wb" ----
            uint32_t aud_offset = (uint32_t)(ftell(f) - movi_data_pos);
            fwrite("01wb", 1, 4, f);
            AviWu32(f, audio_bytes);
            fwrite(audio_buf.data(), 1, audio_bytes, f);
            // audio_bytes is always even (16-bit), no padding needed

            // Add audio index entry
            {
                size_t n = idx1.size();
                idx1.resize(n + 16);
                uint8_t* ep = idx1.data() + n;
                uint32_t aflags = 0;
                memcpy(ep,    "01wb", 4);
                memcpy(ep+4,  &aflags,      4);
                memcpy(ep+8,  &aud_offset,  4);
                memcpy(ep+12, &audio_bytes, 4);
            }
            total_audio_samples += (uint32_t)wanted;

            // Pace to target FPS
            int64_t elapsed = esp_timer_get_time() / 1000 - frame_start;
            int64_t delay = frame_interval_ms - elapsed;
            if (delay > 5) vTaskDelay(pdMS_TO_TICKS(delay));
        }

        // ===== FINALIZE AVI =====

        // Close movi LIST
        long file_end = ftell(f);
        uint32_t movi_sz = (uint32_t)(file_end - movi_size_pos - 4);
        fseek(f, movi_size_pos, SEEK_SET);
        AviWu32(f, movi_sz);
        fseek(f, file_end, SEEK_SET);

        // Write idx1 chunk
        if (!idx1.empty()) {
            fwrite("idx1", 1, 4, f);
            AviWu32(f, (uint32_t)idx1.size());
            fwrite(idx1.data(), 1, idx1.size(), f);
        }

        // Update RIFF size
        file_end = ftell(f);
        uint32_t riff_sz = (uint32_t)(file_end - 8);
        fseek(f, riff_size_pos, SEEK_SET);
        AviWu32(f, riff_sz);

        // Update avih TotalFrames
        fseek(f, avih_frames_pos, SEEK_SET);
        AviWu32(f, total_frames);

        // Update video strh Length
        fseek(f, vstrh_len_pos, SEEK_SET);
        AviWu32(f, total_frames);

        // Update audio strh Length
        fseek(f, astrh_len_pos, SEEK_SET);
        AviWu32(f, total_audio_samples);

        fclose(f);
        ESP_LOGI(TAG, "Video saved: %s (%u frames, %u audio samples)", filepath, total_frames, total_audio_samples);

        // Store filename (strip mount point prefix)
        last_video_name_ = std::string(filepath + strlen(SD_MOUNT_POINT) + 1);

        // Cleanup
        if (pdm_codec_) pdm_codec_->SetAudioTapCallback(nullptr);
        delete[] rec_ring_; rec_ring_ = nullptr;
        record_task_ = nullptr;
        // recording_ was set to false by stop handler (or auto-stop above)
        recording_ = false;
    }

    // ---- Record start/stop HTTP handlers ----
    static esp_err_t HandleRecordStartApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        if (!self->sd_mounted_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"SD card not mounted\"}");
            return ESP_OK;
        }
        if (self->recording_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Already recording\"}");
            return ESP_OK;
        }
        if (self->record_task_ != nullptr) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Previous task still running\"}");
            return ESP_OK;
        }

        self->recording_ = true;
        self->last_video_name_ = "";
        BaseType_t ret = xTaskCreate(RecordingTaskFn, "avi_rec", 8192, self, 3, &self->record_task_);
        if (ret != pdPASS) {
            self->recording_ = false;
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Failed to start task\"}");
            return ESP_OK;
        }

        httpd_resp_sendstr(req, "{\"ok\":true}");
        return ESP_OK;
    }

    static esp_err_t HandleRecordStopApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        if (!self->recording_ && self->record_task_ == nullptr) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Not recording\"}");
            return ESP_OK;
        }

        // Signal recording loop to stop
        self->recording_ = false;

        // Wait for recording task to finish (max 15 seconds)
        int waited = 0;
        while (self->record_task_ != nullptr && waited < 150) {
            vTaskDelay(pdMS_TO_TICKS(100));
            waited++;
        }

        if (self->record_task_ != nullptr) {
            // Timed out — task still running
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Timeout waiting for recording to stop\"}");
            return ESP_OK;
        }

        // Build response with filename
        std::string fname = self->last_video_name_;
        if (fname.empty()) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"No video saved\"}");
            return ESP_OK;
        }
        std::string json = std::string("{\"ok\":true,\"filename\":\"") + fname + "\"}";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    // ---- Telegram send ----
    struct TelegramSendArg {
        XiaoEsp32s3SenseBoard* board;
        SemaphoreHandle_t done;
        bool success;
        char message[128];
    };

    static void TelegramSendTaskFn(void* arg) {
        auto* a = static_cast<TelegramSendArg*>(arg);
        auto result = a->board->SendToTelegramInternal();
        a->success = result.first;
        strncpy(a->message, result.second.c_str(), sizeof(a->message) - 1);
        a->message[sizeof(a->message) - 1] = '\0';
        xSemaphoreGive(a->done);
        vTaskDelete(nullptr);
    }

    std::pair<bool, std::string> SendToTelegramInternal() {
        if (tg_token_.empty() || tg_chat_id_.empty()) {
            return {false, "Bot token or chat_id not configured"};
        }
        if (!camera_) {
            return {false, "Camera not initialized"};
        }

        // Get a FRESH real-time frame (flush stale buffered frames first)
        camera_fb_t* fb = GetFreshCameraFrame();
        if (!fb) {
            return {false, "Camera capture failed"};
        }

        // Build multipart/form-data body
        const std::string boundary = "----XiaoEspTgBoundary";
        std::string body;
        body.reserve(fb->len + 320);
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n";
        body += tg_chat_id_ + "\r\n";
        body += "--" + boundary + "\r\n";
        body += "Content-Disposition: form-data; name=\"photo\"; filename=\"photo.jpg\"\r\n";
        body += "Content-Type: image/jpeg\r\n\r\n";
        body.append(reinterpret_cast<const char*>(fb->buf), fb->len);
        body += "\r\n--" + boundary + "--\r\n";

        esp_camera_fb_return(fb);

        // POST to Telegram Bot API over HTTPS
        auto network = GetNetwork();
        if (!network) {
            return {false, "Network not available"};
        }
        auto http = network->CreateHttp(-1);
        if (!http) {
            return {false, "Failed to create HTTP client"};
        }

        http->SetTimeout(20000);
        http->SetHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
        http->SetContent(std::move(body));

        std::string url = "https://api.telegram.org/bot" + tg_token_ + "/sendPhoto";
        if (!http->Open("POST", url)) {
            return {false, "Failed to connect to Telegram API"};
        }

        int status = http->GetStatusCode();
        std::string response = http->ReadAll();
        http->Close();

        ESP_LOGI(TAG, "Telegram HTTP %d: %.200s", status, response.c_str());
        if (status == 200) {
            return {true, "Photo sent"};
        }
        return {false, "Telegram HTTP " + std::to_string(status)};
    }

    // ---- Telegram HTTP handlers ----
    static esp_err_t HandleTelegramConfigGet(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        std::string json = "{\"ok\":true,\"has_token\":";
        json += self->tg_token_.empty() ? "false" : "true";
        json += ",\"chat_id\":\"";
        json += self->tg_chat_id_;
        json += "\"}";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    static esp_err_t HandleTelegramConfigPost(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        int content_len = req->content_len;
        if (content_len <= 0 || content_len > 512) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Bad request\"}");
            return ESP_OK;
        }
        std::string body(content_len, '\0');
        int received = httpd_req_recv(req, &body[0], content_len);
        if (received <= 0) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Read error\"}");
            return ESP_OK;
        }
        body.resize(received);

        // Minimal JSON field extraction for "token" and "chat_id"
        auto extract = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":\"";
            size_t pos = body.find(search);
            if (pos == std::string::npos) return "";
            pos += search.size();
            size_t end = body.find('"', pos);
            if (end == std::string::npos) return "";
            return body.substr(pos, end - pos);
        };

        std::string token = extract("token");
        std::string chat_id = extract("chat_id");

        Settings cam_settings("camera", true);
        if (!token.empty()) {
            cam_settings.SetString("tg_token", token);
            self->tg_token_ = token;
        }
        if (!chat_id.empty()) {
            cam_settings.SetString("tg_chat_id", chat_id);
            self->tg_chat_id_ = chat_id;
        }

        httpd_resp_sendstr(req, "{\"ok\":true}");
        return ESP_OK;
    }

    static esp_err_t HandleTelegramSendPost(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        TelegramSendArg arg;
        arg.board = self;
        arg.done = xSemaphoreCreateBinary();
        arg.success = false;
        arg.message[0] = '\0';

        if (!arg.done) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Semaphore failed\"}");
            return ESP_OK;
        }

        BaseType_t ret = xTaskCreate(TelegramSendTaskFn, "tg_send", 12288, &arg, 4, nullptr);
        if (ret != pdPASS) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Task start failed\"}");
            return ESP_OK;
        }

        // Block httpd connection while sending; browser shows spinner
        if (xSemaphoreTake(arg.done, pdMS_TO_TICKS(25000)) != pdTRUE) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Timeout\"}");
            return ESP_OK;
        }
        vSemaphoreDelete(arg.done);

        if (arg.success) {
            httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"Photo sent to Telegram\"}");
        } else {
            std::string json = std::string("{\"ok\":false,\"message\":\"") + arg.message + "\"}";
            httpd_resp_sendstr(req, json.c_str());
        }
        return ESP_OK;
    }

    // ---- Camera rotate handler ----
    static esp_err_t HandleCameraRotateApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        if (!self->camera_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Camera not ready\"}");
            return ESP_OK;
        }

        self->camera_rotation_ = (self->camera_rotation_ + 1) % 4;
        int rot = self->camera_rotation_;
        // 0=Normal, 1=HMirror, 2=180deg (H+V), 3=VFlip
        bool hmirror = (rot == 1 || rot == 2);
        bool vflip   = (rot == 2 || rot == 3);
        self->camera_->SetHMirror(hmirror);
        self->camera_->SetVFlip(vflip);

        const char* names[] = {"Normal", "Mirror", "180 do", "Lat doc"};
        std::string json = std::string("{\"ok\":true,\"rotation\":") +
            std::to_string(rot) + ",\"name\":\"" + names[rot] + "\"}";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    // ---- Telegram video send (streaming, no full-file buffering) ----
    struct TelegramVideoSendArg {
        XiaoEsp32s3SenseBoard* board;
        SemaphoreHandle_t done;
        bool success;
        char message[128];
        char filepath[128];
    };

    static void TelegramVideoSendTaskFn(void* arg) {
        auto* a = static_cast<TelegramVideoSendArg*>(arg);
        auto result = a->board->SendVideoToTelegramInternal(std::string(a->filepath));
        a->success = result.first;
        strncpy(a->message, result.second.c_str(), sizeof(a->message) - 1);
        a->message[sizeof(a->message) - 1] = '\0';
        xSemaphoreGive(a->done);
        vTaskDelete(nullptr);
    }

    std::pair<bool, std::string> SendVideoToTelegramInternal(const std::string& video_path) {
        if (tg_token_.empty() || tg_chat_id_.empty()) {
            return {false, "Token/chat_id not configured"};
        }

        // Get file size
        FILE* f = fopen(video_path.c_str(), "rb");
        if (!f) return {false, "Video file not found"};
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (file_size <= 0 || file_size > 49 * 1024 * 1024L) {
            fclose(f);
            return {false, "File size invalid or > 49MB"};
        }

        // Build multipart parts
        const std::string boundary = "----XiaoVidBound";
        std::string part_head = "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
            tg_chat_id_ + "\r\n" +
            "--" + boundary + "\r\n" +
            "Content-Disposition: form-data; name=\"document\"; filename=\"video.avi\"\r\n"
            "Content-Type: video/x-msvideo\r\n\r\n";
        std::string part_tail = "\r\n--" + boundary + "--\r\n";

        long total_len = (long)part_head.size() + file_size + (long)part_tail.size();

        std::string url = "https://api.telegram.org/bot" + tg_token_ + "/sendDocument";
        std::string ct  = "multipart/form-data; boundary=" + boundary;

        esp_http_client_config_t cfg = {};
        cfg.url            = url.c_str();
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.timeout_ms     = 120000;
        cfg.buffer_size    = 4096;
        cfg.buffer_size_tx = 4096;

        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { fclose(f); return {false, "HTTP client init failed"}; }

        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", ct.c_str());

        esp_err_t err = esp_http_client_open(client, total_len);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client);
            fclose(f);
            return {false, std::string("Connect failed: ") + esp_err_to_name(err)};
        }

        // Write multipart header
        int wr = esp_http_client_write(client, part_head.c_str(), (int)part_head.size());
        if (wr < 0) {
            esp_http_client_cleanup(client);
            fclose(f);
            return {false, "Write header failed"};
        }

        // Stream file in 4KB chunks
        char chunk[4096];
        size_t n;
        bool write_ok = true;
        while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
            if (esp_http_client_write(client, chunk, (int)n) < 0) {
                write_ok = false;
                break;
            }
        }
        fclose(f);

        if (!write_ok) {
            esp_http_client_cleanup(client);
            return {false, "File stream write error"};
        }

        // Write multipart tail
        esp_http_client_write(client, part_tail.c_str(), (int)part_tail.size());

        // Fetch response
        int content_len = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);

        std::string resp_body;
        if (content_len > 0 && content_len < 2048) {
            resp_body.resize(content_len);
            esp_http_client_read_response(client, &resp_body[0], content_len);
        } else {
            char rbuf[512] = {};
            int rn = esp_http_client_read_response(client, rbuf, (int)sizeof(rbuf) - 1);
            if (rn > 0) resp_body = rbuf;
        }
        esp_http_client_cleanup(client);

        ESP_LOGI(TAG, "Telegram video HTTP %d: %.200s", status, resp_body.c_str());
        if (status == 200) return {true, "Video sent"};
        return {false, "Telegram HTTP " + std::to_string(status)};
    }

    static esp_err_t HandleTelegramSendVideoPost(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

        if (!self->sd_mounted_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"SD card not mounted\"}");
            return ESP_OK;
        }

        // Find the most recent .avi file (filenames are vid_YYYYMMDD_HHMMSS.avi → alphabetical = chronological)
        DIR* dir = opendir(SD_MOUNT_POINT);
        if (!dir) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Cannot open SD card\"}");
            return ESP_OK;
        }
        std::string latest_name;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name.size() > 4 && name.substr(name.size() - 4) == ".avi") {
                if (name > latest_name) latest_name = name;
            }
        }
        closedir(dir);

        if (latest_name.empty()) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"No .avi file found on SD\"}");
            return ESP_OK;
        }

        TelegramVideoSendArg arg;
        arg.board = self;
        arg.done = xSemaphoreCreateBinary();
        arg.success = false;
        arg.message[0] = '\0';
        snprintf(arg.filepath, sizeof(arg.filepath), SD_MOUNT_POINT "/%s", latest_name.c_str());

        if (!arg.done) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Semaphore failed\"}");
            return ESP_OK;
        }

        // Large stack needed for file buffering
        BaseType_t ret = xTaskCreate(TelegramVideoSendTaskFn, "tg_vid", 16384, &arg, 4, nullptr);
        if (ret != pdPASS) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Task start failed\"}");
            return ESP_OK;
        }

        // Wait up to 120 seconds for upload to complete
        if (xSemaphoreTake(arg.done, pdMS_TO_TICKS(120000)) != pdTRUE) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Timeout uploading video\"}");
            return ESP_OK;
        }
        vSemaphoreDelete(arg.done);

        if (arg.success) {
            std::string json = std::string("{\"ok\":true,\"message\":\"Da gui video: ") + latest_name + "\"}";
            httpd_resp_sendstr(req, json.c_str());
        } else {
            std::string json = std::string("{\"ok\":false,\"message\":\"") + arg.message + "\"}";
            httpd_resp_sendstr(req, json.c_str());
        }
        return ESP_OK;
    }

    // ---- Telegram send specific file (photo or video) from SD ----
    struct TelegramSendFileArg {
        XiaoEsp32s3SenseBoard* board;
        SemaphoreHandle_t done;
        bool success;
        char message[128];
        char filepath[128];
        bool is_photo;
    };

    static void TelegramSendFileTaskFn(void* arg) {
        auto* a = static_cast<TelegramSendFileArg*>(arg);
        auto result = a->board->SendFileToTelegramInternal(std::string(a->filepath), a->is_photo);
        a->success = result.first;
        strncpy(a->message, result.second.c_str(), sizeof(a->message)-1);
        a->message[sizeof(a->message)-1] = '\0';
        xSemaphoreGive(a->done);
        vTaskDelete(nullptr);
    }

    std::pair<bool, std::string> SendFileToTelegramInternal(const std::string& filepath, bool is_photo) {
        if (tg_token_.empty() || tg_chat_id_.empty()) return {false, "Token/chat_id not configured"};
        FILE* f = fopen(filepath.c_str(), "rb");
        if (!f) return {false, "File not found"};
        fseek(f, 0, SEEK_END);
        long file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        long max_size = is_photo ? 10L*1024*1024 : 49L*1024*1024;
        if (file_size <= 0 || file_size > max_size) { fclose(f); return {false, "File size invalid"}; }
        const std::string boundary = "----XiaoFileBound";
        const char* field = is_photo ? "photo"     : "document";
        const char* fname = is_photo ? "photo.jpg"  : "video.avi";
        const char* fct   = is_photo ? "image/jpeg" : "video/x-msvideo";
        const char* api   = is_photo ? "sendPhoto"  : "sendDocument";
        std::string part_head =
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
            tg_chat_id_ + "\r\n" +
            "--" + boundary + "\r\n"
            "Content-Disposition: form-data; name=\"" + field + "\"; filename=\"" + fname + "\"\r\n"
            "Content-Type: " + fct + "\r\n\r\n";
        std::string part_tail = "\r\n--" + boundary + "--\r\n";
        long total_len = (long)part_head.size() + file_size + (long)part_tail.size();
        std::string url = "https://api.telegram.org/bot" + tg_token_ + "/" + api;
        std::string ct  = "multipart/form-data; boundary=" + boundary;
        esp_http_client_config_t cfg = {};
        cfg.url = url.c_str();
        cfg.crt_bundle_attach = esp_crt_bundle_attach;
        cfg.timeout_ms = is_photo ? 60000 : 120000;
        cfg.buffer_size = 4096;
        cfg.buffer_size_tx = 4096;
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) { fclose(f); return {false, "HTTP client init failed"}; }
        esp_http_client_set_method(client, HTTP_METHOD_POST);
        esp_http_client_set_header(client, "Content-Type", ct.c_str());
        esp_err_t err = esp_http_client_open(client, total_len);
        if (err != ESP_OK) {
            esp_http_client_cleanup(client); fclose(f);
            return {false, std::string("Connect failed: ") + esp_err_to_name(err)};
        }
        esp_http_client_write(client, part_head.c_str(), (int)part_head.size());
        char chunk[4096];
        size_t n;
        bool write_ok = true;
        while ((n = fread(chunk, 1, sizeof(chunk), f)) > 0) {
            if (esp_http_client_write(client, chunk, (int)n) < 0) { write_ok = false; break; }
        }
        fclose(f);
        if (!write_ok) { esp_http_client_cleanup(client); return {false, "Stream write error"}; }
        esp_http_client_write(client, part_tail.c_str(), (int)part_tail.size());
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        char rbuf[512] = {};
        esp_http_client_read_response(client, rbuf, sizeof(rbuf)-1);
        esp_http_client_cleanup(client);
        ESP_LOGI(TAG, "Telegram sendfile HTTP %d: %.200s", status, rbuf);
        if (status == 200) return {true, "Sent"};
        return {false, "Telegram HTTP " + std::to_string(status)};
    }

    static esp_err_t HandleTelegramSendFilePost(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        if (!self->sd_mounted_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"SD not mounted\"}");
            return ESP_OK;
        }
        int content_len = req->content_len;
        if (content_len <= 0 || content_len > 256) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Bad request\"}");
            return ESP_OK;
        }
        std::string body(content_len, '\0');
        int received = httpd_req_recv(req, &body[0], content_len);
        if (received <= 0) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Read error\"}");
            return ESP_OK;
        }
        body.resize(received);
        auto extract = [&](const std::string& key) -> std::string {
            std::string search = "\"" + key + "\":\"";
            size_t pos = body.find(search);
            if (pos == std::string::npos) return "";
            pos += search.size();
            size_t end = body.find('"', pos);
            if (end == std::string::npos) return "";
            return body.substr(pos, end - pos);
        };
        std::string filename = extract("filename");
        if (filename.empty() || strchr(filename.c_str(), '/') || filename.find("..") != std::string::npos) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Invalid filename\"}");
            return ESP_OK;
        }
        bool is_photo = (filename.size() > 4 && filename.substr(filename.size()-4) == ".jpg");
        bool is_video = (filename.size() > 4 && filename.substr(filename.size()-4) == ".avi");
        if (!is_photo && !is_video) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Unsupported file type\"}");
            return ESP_OK;
        }
        TelegramSendFileArg arg;
        arg.board = self;
        arg.done = xSemaphoreCreateBinary();
        arg.success = false;
        arg.message[0] = '\0';
        arg.is_photo = is_photo;
        snprintf(arg.filepath, sizeof(arg.filepath), SD_MOUNT_POINT "/%s", filename.c_str());
        if (!arg.done) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Semaphore failed\"}");
            return ESP_OK;
        }
        BaseType_t ret = xTaskCreate(TelegramSendFileTaskFn, "tg_file", 16384, &arg, 4, nullptr);
        if (ret != pdPASS) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Task start failed\"}");
            return ESP_OK;
        }
        TickType_t tout = is_photo ? pdMS_TO_TICKS(60000) : pdMS_TO_TICKS(120000);
        if (xSemaphoreTake(arg.done, tout) != pdTRUE) {
            vSemaphoreDelete(arg.done);
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Timeout\"}");
            return ESP_OK;
        }
        vSemaphoreDelete(arg.done);
        if (arg.success) {
            std::string json = std::string("{\"ok\":true,\"message\":\"Da gui: ") + filename + "\"}";
            httpd_resp_sendstr(req, json.c_str());
        } else {
            std::string json = std::string("{\"ok\":false,\"message\":\"") + arg.message + "\"}";
            httpd_resp_sendstr(req, json.c_str());
        }
        return ESP_OK;
    }

    static esp_err_t HandleDeleteFileApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        if (!self->sd_mounted_) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"SD not mounted\"}");
            return ESP_OK;
        }
        // Extract filename from query parameter
        char filename_buf[256] = {0};
        if (httpd_req_get_url_query_str(req, nullptr, 0) == ESP_ERR_HTTPD_INVALID_REQ) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Missing filename\"}");
            return ESP_OK;
        }
        char query[512] = {0};
        size_t query_len = httpd_req_get_url_query_len(req) + 1;
        if (query_len > sizeof(query) || httpd_req_get_url_query_str(req, query, query_len) != ESP_OK) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Query too long\"}");
            return ESP_OK;
        }
        // Extract filename parameter: filename=...
        if (httpd_query_key_value(query, "filename", filename_buf, sizeof(filename_buf) - 1) != ESP_OK) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Missing filename parameter\"}");
            return ESP_OK;
        }
        std::string filename(filename_buf);
        // Validate filename: no path traversal, valid extension
        if (filename.empty() || strchr(filename.c_str(), '/') || filename.find("..") != std::string::npos) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Invalid filename\"}");
            return ESP_OK;
        }
        bool is_jpg = (filename.size() > 4 && filename.substr(filename.size()-4) == ".jpg");
        bool is_avi = (filename.size() > 4 && filename.substr(filename.size()-4) == ".avi");
        if (!is_jpg && !is_avi) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Unsupported file type\"}");
            return ESP_OK;
        }
        // Build full path
        std::string filepath = SD_MOUNT_POINT;
        filepath += "/";
        filepath += filename;
        // Delete the file
        if (remove(filepath.c_str()) != 0) {
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"Delete failed\"}");
            return ESP_OK;
        }
        std::string json = std::string("{\"ok\":true,\"message\":\"Deleted: ") + filename + "\"}";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    void StopPhotoWebServer() {
        if (stream_server_ != nullptr) {
            httpd_stop(stream_server_);
            stream_server_ = nullptr;
        }
        if (server_ != nullptr) {
            httpd_stop(server_);
            server_ = nullptr;
            ESP_LOGI(TAG, "Photo web server stopped");
        }
    }

    // --- HTTP server static handlers ---
    static esp_err_t HandleGallery(httpd_req_t* req) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_sendstr(req, GALLERY_HTML);
        return ESP_OK;
    }

    static esp_err_t HandlePhotoFile(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        if (!self->sd_mounted_) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "SD card not mounted");
            return ESP_FAIL;
        }
        // req->uri is like "/photo/<filename>"
        const char* prefix = "/photo/";
        const char* uri = req->uri;
        if (strncmp(uri, prefix, strlen(prefix)) != 0) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        const char* filename = uri + strlen(prefix);
        // Prevent path traversal
        if (strchr(filename, '/') || strstr(filename, "..")) {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid filename");
            return ESP_FAIL;
        }
        char filepath[80];
        snprintf(filepath, sizeof(filepath), SD_MOUNT_POINT "/%s", filename);

        FILE* f = fopen(filepath, "rb");
        if (!f) {
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        // Set correct content-type by extension
        {
            std::string fn_ext(filename);
            if (fn_ext.size() > 4 && fn_ext.substr(fn_ext.size()-4) == ".avi") {
                httpd_resp_set_type(req, "video/x-msvideo");
            } else {
                httpd_resp_set_type(req, "image/jpeg");
            }
        }
        char* buf = static_cast<char*>(malloc(4096));
        if (!buf) {
            fclose(f);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        size_t read_len;
        esp_err_t ret = ESP_OK;
        while ((read_len = fread(buf, 1, 4096, f)) > 0) {
            if (httpd_resp_send_chunk(req, buf, read_len) != ESP_OK) {
                ret = ESP_FAIL;
                break;
            }
        }
        fclose(f);
        free(buf);
        httpd_resp_send_chunk(req, nullptr, 0);
        return ret;
    }

    static esp_err_t HandlePhotosApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        if (!self->sd_mounted_) {
            httpd_resp_sendstr(req, "[]");
            return ESP_OK;
        }
        DIR* dir = opendir(SD_MOUNT_POINT);
        if (!dir) {
            httpd_resp_sendstr(req, "[]");
            return ESP_OK;
        }
        std::string json = "[";
        bool first = true;
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name.size() > 4 && name.substr(name.size() - 4) == ".jpg") {
                if (!first) json += ",";
                json += "\"" + name + "\"";
                first = false;
            }
        }
        closedir(dir);
        json += "]";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    static esp_err_t HandleFilesApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        if (!self->sd_mounted_) { httpd_resp_sendstr(req, "[]"); return ESP_OK; }
        DIR* dir2 = opendir(SD_MOUNT_POINT);
        if (!dir2) { httpd_resp_sendstr(req, "[]"); return ESP_OK; }
        std::string json2 = "[";
        bool first2 = true;
        struct dirent* entry2;
        while ((entry2 = readdir(dir2)) != nullptr) {
            std::string name2(entry2->d_name);
            bool is_jpg = (name2.size() > 4 && name2.substr(name2.size()-4) == ".jpg");
            bool is_avi = (name2.size() > 4 && name2.substr(name2.size()-4) == ".avi");
            if (is_jpg || is_avi) {
                if (!first2) json2 += ",";
                json2 += "{\"name\":\"" + name2 + "\",\"type\":\"" + (is_jpg ? "photo" : "video") + "\"}";
                first2 = false;
            }
        }
        closedir(dir2);
        json2 += "]";
        httpd_resp_sendstr(req, json2.c_str());
        return ESP_OK;
    }

    static esp_err_t HandleCaptureApi(httpd_req_t* req) {
        auto* self = static_cast<XiaoEsp32s3SenseBoard*>(req->user_ctx);
        httpd_resp_set_type(req, "application/json");

        std::string result = self->CaptureAndSave();
        if (result.rfind("Photo saved: ", 0) == 0) {
            std::string filename = result.substr(strlen("Photo saved: "));
            std::string json = std::string("{\"ok\":true,\"filename\":\"") + filename + "\"}";
            httpd_resp_sendstr(req, json.c_str());
            return ESP_OK;
        }

        std::string escaped;
        escaped.reserve(result.size());
        for (char ch : result) {
            if (ch == '\\' || ch == '"') {
                escaped.push_back('\\');
            }
            escaped.push_back(ch);
        }
        std::string json = std::string("{\"ok\":false,\"message\":\"") + escaped + "\"}";
        httpd_resp_sendstr(req, json.c_str());
        return ESP_OK;
    }

    static esp_err_t HandleStreamApi(httpd_req_t* req) {
        httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=" STREAM_BOUNDARY);
        httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
        httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

        char part_buf[80];
        while (true) {
            camera_fb_t* fb = esp_camera_fb_get();
            if (!fb) {
                ESP_LOGE(TAG, "Stream: camera capture failed");
                break;
            }

            // Skip invalid JPEG (must start with SOI marker 0xFF 0xD8)
            if (fb->format != PIXFORMAT_JPEG || fb->len < 4 ||
                fb->buf[0] != 0xFF || fb->buf[1] != 0xD8) {
                esp_camera_fb_return(fb);
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }

            int hlen = snprintf(part_buf, sizeof(part_buf),
                "--" STREAM_BOUNDARY "\r\nContent-Type: image/jpeg\r\nContent-Length: %zu\r\n\r\n",
                fb->len);

            esp_err_t res = httpd_resp_send_chunk(req, part_buf, hlen);
            if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
            if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);

            esp_camera_fb_return(fb);

            if (res != ESP_OK) {
                break; // Client disconnected
            }
            vTaskDelay(pdMS_TO_TICKS(30)); // ~33fps max
        }
        return ESP_OK;
    }

    void InitializePhotoWebServer() {
        if (server_ != nullptr) {
            return;
        }

        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.uri_match_fn = httpd_uri_match_wildcard;
        config.lru_purge_enable = true;
        config.stack_size = 8192;
        config.max_open_sockets = 3; // Limit sockets: only 1 browser needed
        config.max_uri_handlers = 16; // default 8; we register 13+ handlers

        if (httpd_start(&server_, &config) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start photo web server");
            return;
        }

        httpd_uri_t gallery_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = HandleGallery,
            .user_ctx = this,
        };
        httpd_uri_t photo_uri = {
            .uri = "/photo/*",
            .method = HTTP_GET,
            .handler = HandlePhotoFile,
            .user_ctx = this,
        };
        httpd_uri_t api_uri = {
            .uri = "/api/photos",
            .method = HTTP_GET,
            .handler = HandlePhotosApi,
            .user_ctx = this,
        };
        httpd_uri_t capture_uri = {
            .uri = "/api/capture",
            .method = HTTP_POST,
            .handler = HandleCaptureApi,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &gallery_uri);
        httpd_register_uri_handler(server_, &photo_uri);
        httpd_register_uri_handler(server_, &api_uri);
        httpd_register_uri_handler(server_, &capture_uri);

        httpd_uri_t rec_start_uri = {
            .uri = "/api/record/start",
            .method = HTTP_POST,
            .handler = HandleRecordStartApi,
            .user_ctx = this,
        };
        httpd_uri_t rec_stop_uri = {
            .uri = "/api/record/stop",
            .method = HTTP_POST,
            .handler = HandleRecordStopApi,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &rec_start_uri);
        httpd_register_uri_handler(server_, &rec_stop_uri);

        httpd_uri_t tg_cfg_get_uri = {
            .uri = "/api/telegram/config",
            .method = HTTP_GET,
            .handler = HandleTelegramConfigGet,
            .user_ctx = this,
        };
        httpd_uri_t tg_cfg_post_uri = {
            .uri = "/api/telegram/config",
            .method = HTTP_POST,
            .handler = HandleTelegramConfigPost,
            .user_ctx = this,
        };
        httpd_uri_t tg_send_uri = {
            .uri = "/api/telegram/send",
            .method = HTTP_POST,
            .handler = HandleTelegramSendPost,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &tg_cfg_get_uri);
        httpd_register_uri_handler(server_, &tg_cfg_post_uri);
        httpd_register_uri_handler(server_, &tg_send_uri);

        httpd_uri_t camera_rotate_uri = {
            .uri = "/api/camera/rotate",
            .method = HTTP_POST,
            .handler = HandleCameraRotateApi,
            .user_ctx = this,
        };
        httpd_uri_t tg_vid_uri = {
            .uri = "/api/telegram/sendvideo",
            .method = HTTP_POST,
            .handler = HandleTelegramSendVideoPost,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &camera_rotate_uri);
        httpd_register_uri_handler(server_, &tg_vid_uri);

        httpd_uri_t files_uri = {
            .uri = "/api/files",
            .method = HTTP_GET,
            .handler = HandleFilesApi,
            .user_ctx = this,
        };
        httpd_uri_t tg_sendfile_uri = {
            .uri = "/api/telegram/sendfile",
            .method = HTTP_POST,
            .handler = HandleTelegramSendFilePost,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &files_uri);
        httpd_register_uri_handler(server_, &tg_sendfile_uri);

        httpd_uri_t delete_uri = {
            .uri = "/api/files",
            .method = HTTP_DELETE,
            .handler = HandleDeleteFileApi,
            .user_ctx = this,
        };
        httpd_register_uri_handler(server_, &delete_uri);

        // Start MJPEG stream server on port 81
        httpd_config_t stream_cfg = HTTPD_DEFAULT_CONFIG();
        stream_cfg.server_port = 81;
        stream_cfg.ctrl_port = 32769;
        stream_cfg.stack_size = 8192;
        stream_cfg.max_open_sockets = 3;
        if (httpd_start(&stream_server_, &stream_cfg) == ESP_OK) {
            httpd_uri_t stream_uri = {
                .uri = "/stream",
                .method = HTTP_GET,
                .handler = HandleStreamApi,
                .user_ctx = this,
            };
            httpd_register_uri_handler(stream_server_, &stream_uri);
            ESP_LOGI(TAG, "Stream server started on port 81");
        } else {
            ESP_LOGE(TAG, "Failed to start stream server");
        }

        ESP_LOGI(TAG, "Photo web server started on port %d", config.server_port);
    }

    void InitializeSdCard() {
        spi_bus_config_t bus_cfg = {};
        bus_cfg.mosi_io_num = SD_MOSI;
        bus_cfg.miso_io_num = SD_MISO;
        bus_cfg.sclk_io_num = SD_CLK;
        bus_cfg.quadwp_io_num = -1;
        bus_cfg.quadhd_io_num = -1;
        bus_cfg.max_transfer_sz = 4096;

        esp_err_t ret = spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
            return;
        }

        sdspi_device_config_t slot_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
        slot_cfg.gpio_cs = SD_CS;
        slot_cfg.host_id = SD_SPI_HOST;

        esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
            .format_if_mount_failed = false,
            .max_files = 20,
            .allocation_unit_size = 16 * 1024,
        };

        sdmmc_card_t* card = nullptr;
        sdmmc_host_t host = SDSPI_HOST_DEFAULT();
        host.max_freq_khz = 10000; // 10MHz — more reliable than default 20MHz
        for (int retry = 0; retry <= 3; retry++) {
            ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_cfg, &mount_cfg, &card);
            if (ret == ESP_OK) break;
            ESP_LOGW(TAG, "SD mount attempt %d failed: %s", retry + 1, esp_err_to_name(ret));
            if (retry < 3) vTaskDelay(pdMS_TO_TICKS(300));
        }
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SD card mount failed: %s", esp_err_to_name(ret));
            sd_mounted_ = false;
        } else {
            ESP_LOGI(TAG, "SD card mounted at %s", SD_MOUNT_POINT);
            sd_mounted_ = true;
        }
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 0;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_JPEG;
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        camera_->SetHMirror(false);
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    std::string CaptureAndSave() {
        if (!sd_mounted_) {
            return "SD card not mounted";
        }
        if (camera_ == nullptr) {
            return "Camera not initialized";
        }

        // Get a FRESH real-time frame (flush stale buffered frames first)
        camera_fb_t* fb = GetFreshCameraFrame();
        if (!fb) {
            return "Camera capture failed: no valid JPEG";
        }

        // Build filename: /sdcard/photo_YYYYMMDD_HHMMSS.jpg
        time_t now = time(nullptr);
        struct tm t;
        localtime_r(&now, &t);
        char filename[64];
        snprintf(filename, sizeof(filename),
            SD_MOUNT_POINT "/photo_%04d%02d%02d_%02d%02d%02d.jpg",
            t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
            t.tm_hour, t.tm_min, t.tm_sec);

        FILE* f = fopen(filename, "wb");
        if (!f) {
            ESP_LOGE(TAG, "Cannot open file %s: errno=%d (%s)", filename, errno, strerror(errno));
            esp_camera_fb_return(fb);
            return std::string("Cannot open file: ") + filename;
        }

        size_t written = fwrite(fb->buf, 1, fb->len, f);
        fclose(f);
        esp_camera_fb_return(fb);

        if (written != fb->len) {
            return std::string("Write incomplete: ") + filename;
        }

        ESP_LOGI(TAG, "Photo saved: %s (%zu bytes)", filename, written);
        return std::string("Photo saved: ") + (filename + strlen(SD_MOUNT_POINT) + 1);
    }

    void InitializeTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("camera.capture_to_sdcard",
            "Capture a photo from the camera and save it to the SD card as a JPEG file. "
            "Returns the filename if successful.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return CaptureAndSave();
            });

        mcp.AddTool("sdcard.list_photos",
            "List all JPEG photos saved on the SD card.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                if (!sd_mounted_) {
                    return std::string("SD card not mounted");
                }
                DIR* dir = opendir(SD_MOUNT_POINT);
                if (!dir) {
                    return std::string("Cannot open SD card directory");
                }
                std::string result;
                struct dirent* entry;
                int count = 0;
                while ((entry = readdir(dir)) != nullptr) {
                    std::string name(entry->d_name);
                    if (name.size() > 4 &&
                        name.substr(name.size() - 4) == ".jpg") {
                        result += name + "\n";
                        count++;
                    }
                }
                closedir(dir);
                if (count == 0) {
                    return std::string("No photos found on SD card");
                }
                return result;
            });
    }

public:
    XiaoEsp32s3SenseBoard() :
        boot_button_(BOOT_BUTTON_GPIO) {
        pdm_codec_ = new NoAudioCodecSimplexPdm(
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_DIN);
        InitializeSdCard();
        InitializeCamera();
        InitializeButtons();
        InitializeTools();

        // Load persisted Telegram settings
        Settings cam_settings("camera", false);
        tg_token_ = cam_settings.GetString("tg_token", "");
        tg_chat_id_ = cam_settings.GetString("tg_chat_id", "");
    }

    virtual void SetNetworkEventCallback(NetworkEventCallback callback) override {
        WifiBoard::SetNetworkEventCallback([this, callback = std::move(callback)](NetworkEvent event, const std::string& data) {
            switch (event) {
                case NetworkEvent::Connected:
                    InitializePhotoWebServer();
                    break;
                case NetworkEvent::Disconnected:
                case NetworkEvent::WifiConfigModeEnter:
                    StopPhotoWebServer();
                    break;
                default:
                    break;
            }

            if (callback) {
                callback(event, data);
            }
        });
    }

    virtual AudioCodec* GetAudioCodec() override {
        return pdm_codec_;
    }

    virtual Display* GetDisplay() override {
        static NoDisplay display;
        return &display;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }
};

DECLARE_BOARD(XiaoEsp32s3SenseBoard);

