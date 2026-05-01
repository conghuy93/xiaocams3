#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "esp32_camera.h"
#include "mcp_server.h"

#include <esp_log.h>
#include <esp_http_server.h>
#include <esp_timer.h>
#include <driver/spi_common.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>
#include <driver/sdspi_host.h>
#include <ctime>
#include <cstring>
#include <cerrno>
#include <dirent.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <functional>
#include <vector>

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
".card img{width:100%;height:140px;object-fit:cover;display:block}"
".name{font-size:11px;padding:4px 6px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}"
".lb{display:none;position:fixed;inset:0;background:rgba(0,0,0,.93);z-index:9;align-items:center;justify-content:center;flex-direction:column}"
".lb.on{display:flex}"
".lb img{max-width:96vw;max-height:88vh;object-fit:contain}"
".x{position:absolute;top:8px;right:16px;font-size:28px;cursor:pointer;color:#fff;background:none;border:none}"
".fn{margin-top:8px;color:#aaa;font-size:12px}"
".btn{padding:8px 20px;background:#4fc3f7;color:#111;border:none;border-radius:6px;cursor:pointer;font-size:14px}"
".btn.alt{background:#f6c344}"
".btn[disabled]{background:#555;color:#bbb;cursor:not-allowed}"
"</style></head><body>"
"<h1>&#128247; XIAO Camera</h1>"
"<div class=toolbar>"
"<button class=btn onclick='capturePhoto()'>Chup anh</button>"
"<button class=btn alt onclick='loadPhotos()'>Refresh</button>"
"<button id=sb class=btn onclick='toggleStream()' style='background:#4caf50;color:#fff'>&#9654; Live</button>"
"<button id=rb class=btn onclick='toggleRecord()' style='background:#e53935;color:#fff'>&#9679; Quay video</button>"
"</div>"
"<div id=sv style='display:none;text-align:center;padding:10px 10px 0'>"
"<img id=si style='max-width:100%;max-height:70vh;border-radius:6px;display:block;margin:0 auto'>"
"<br><button class=btn style='margin-top:8px;background:#c62828;color:#fff' onclick='stopStream()'>&#9209; Dung stream</button>"
"</div>"
"<div id=info>Loading&#8230;</div>"
"<div class=grid id=g></div>"
"<div class=lb id=lb>"
"<button class=x onclick=\"document.getElementById('lb').classList.remove('on')\">\u2715</button>"
"<img id=lbimg src=''><div class=fn id=lbname></div></div>"
"<script>"
"let busy=false,streaming=false,recording=false,recTimer=null,recSecs=0;"
"function stopStream(){streaming=false;document.getElementById('sv').style.display='none';document.getElementById('si').src='';document.getElementById('sb').textContent='\u25b6 Live';}"
"function toggleStream(){if(!streaming){streaming=true;document.getElementById('sv').style.display='block';document.getElementById('si').src='http://'+location.hostname+':81/stream?t='+Date.now();document.getElementById('sb').textContent='\u23f9 Stop';}else stopStream();}"
"function viewPhoto(n){document.getElementById('lbimg').src='/photo/'+n;document.getElementById('lbname').textContent=n;document.getElementById('lb').classList.add('on');}"
"document.getElementById('lb').addEventListener('click',function(e){if(e.target===this)this.classList.remove('on');});"
"function setInfo(t){document.getElementById('info').textContent=t;}"
"function toggleRecord(){"
"var rb=document.getElementById('rb');"
"if(recording){"
"recording=false;clearInterval(recTimer);"
"rb.textContent='\u23f9 Dung...';rb.disabled=true;"
"setInfo('Dang luu video...');"
"fetch('/api/record/stop',{method:'POST'}).then(r=>r.json()).then(d=>{"
"rb.disabled=false;rb.textContent='\u25cf Quay video';rb.style.background='#e53935';"
"if(d.ok)setInfo('Da luu: '+d.filename);else setInfo('Loi: '+(d.message||'unknown'));"
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
"function capturePhoto(){if(busy)return;busy=true;setInfo('Dang chup anh...');fetch('/api/capture',{method:'POST'}).then(r=>r.json()).then(data=>{if(!data.ok)throw new Error(data.message||'Capture failed');setInfo('Da luu: '+data.filename);return loadPhotos();}).catch(err=>{setInfo(err.message||'Chup anh that bai');}).finally(()=>{busy=false;});}"
"function loadPhotos(){"
"setInfo('Loading&#8230;');"
"document.getElementById('g').innerHTML='';"
"fetch('/api/photos').then(r=>r.json()).then(photos=>{"
"const info=document.getElementById('info');const g=document.getElementById('g');"
"if(!photos.length){info.textContent='No photos yet. Say \"ch\u1ee5p \u1ea3nh\" to take one!';return;}"
"info.textContent=photos.length+' photo(s) \u2014 tap to view full size';"
"photos.slice().reverse().forEach(n=>{"
"const d=document.createElement('div');d.className='card';d.onclick=()=>viewPhoto(n);"
"d.innerHTML='<img src=/photo/'+n+' loading=lazy><div class=name>'+n+'</div>';"
"g.appendChild(d);});}).catch(()=>{document.getElementById('info').textContent='Failed to load photos';});}"
"loadPhotos();"
"</script></body></html>";


class XiaoEsp32s3SenseBoard : public WifiBoard {
private:
    Button boot_button_;
    Esp32Camera* camera_ = nullptr;
    NoAudioCodecSimplexPdm* pdm_codec_ = nullptr;
    bool sd_mounted_ = false;
    httpd_handle_t server_ = nullptr;
    httpd_handle_t stream_server_ = nullptr;

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
        if (pdm_codec_) {
            pdm_codec_->SetAudioTapCallback([this](const int16_t* buf, int n) {
                portENTER_CRITICAL(&rec_ring_mux_);
                for (int i = 0; i < n; i++) {
                    size_t next_wr = (rec_ring_wr_ + 1) % REC_RING_SAMPLES;
                    if (next_wr != rec_ring_rd_) {
                        rec_ring_[rec_ring_wr_] = buf[i];
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
            int64_t audio_deadline = frame_start + frame_interval_ms;

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
                    vTaskDelay(pdMS_TO_TICKS(3));
                }
            }
            // Pad with silence if we got less than wanted
            for (int i = got; i < wanted; i++) audio_buf[i] = 0;

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
        httpd_resp_set_type(req, "image/jpeg");
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

        // Get JPEG frame — retry up to 5 times to skip invalid warm-up frames
        camera_fb_t* fb = nullptr;
        for (int retry = 0; retry < 5; retry++) {
            fb = esp_camera_fb_get();
            if (fb && fb->len > 4 && fb->buf[0] == 0xFF && fb->buf[1] == 0xD8) {
                break; // valid JPEG
            }
            if (fb) { esp_camera_fb_return(fb); fb = nullptr; }
            vTaskDelay(pdMS_TO_TICKS(50));
        }
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

