#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <iio.h>
#include <utils/optionlist.h>
#include <algorithm>
#include <regex>

#define FULL_GAINTBL_MAX_NUM 83
#define CONCAT(a, b)         ((std::string(a) + b).c_str())

const unsigned short full_gain[FULL_GAINTBL_MAX_NUM] = {
    0x0000,
    0x0001,
    0x0002,
    0x0003,
    0x0004,
    0x0005,
    0x0006,
    0x0021,
    0x0022,
    0x0023,
    0x0024,
    0x0101,
    0x0102,
    0x0103,
    0x0104,
    0x0105,
    0x0106,
    0x0121,
    0x0122,
    0x0123,
    0x0124,
    0x0125,
    0x0126,
    0x0201,
    0x0202,
    0x0203,
    0x0204,
    0x0205,
    0x0206,
    0x0221,
    0x0222,
    0x0223,
    0x0224,
    0x0301,
    0x0302,
    0x0303,
    0x0304,
    0x0305,
    0x0306,
    0x0321,
    0x0322,
    0x0323,
    0x0324,
    0x0325,
    0x0326,
    0x0381,
    0x0382,
    0x0383,
    0x0384,
    0x0385,
    0x0386,
    0x03A1,
    0x03A2,
    0x03A3,
    0x03A4,
    0x03A5,
    0x03A6,
    0x03C1,
    0x03C2,
    0x03C3,
    0x03C4,
    0x03C5,
    0x03E1,
    0x03E2,
    0x03E3,
    0x03E4,
    0x03E5,
    0x03E6,
    0x03D1,
    0x03D2,
    0x03D3,
    0x03D4,
    0x03D5,
    0x03F1,
    0x03F2,
    0x03F3,
    0x03F4,
    0x03F5,
    0x03F6,
    0x03F7,
    0x03F8,
    0x03F9,
    0x03FA,
};

SDRPP_MOD_INFO{
    /* Name:            */ "sdrv1_source",
    /* Description:     */ "SDRv1 source module for SDR++",
    /* Author:          */ "leonard.wang@njavc.com",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AvcSDRSourceModule : public ModuleManager::Instance {
public:
    AvcSDRSourceModule(std::string name) {
        this->name = name;

        // Define valid samplerates
        for (int sr = 1000000; sr <= 61440000; sr += 500000) {
            samplerates.define(sr, getBandwdithScaled(sr), sr);
        }
        samplerates.define(61440000, getBandwdithScaled(61440000.0), 61440000.0);

        // // Define valid bandwidths
        bandwidths.define(0, "Auto", 0);
        for (int bw = 1000000.0; bw <= 52000000; bw += 500000) {
            bandwidths.define(bw, getBandwdithScaled(bw), bw);
        }

        // // Define gain modes
        // RFPort.define("LNA_A", "LNA_A", "0");
        // RFPort.define("LNA_B", "LNA_B", "1");
        // RFPort.define("LNA_C", "LNA_C", "2");

        // Register source
        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("SDRv1", &handler);
    }

    ~AvcSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SDRv1");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = true;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->samplerate);
        flog::info("AvcSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        flog::info("AvcSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        if (_this->running) {
            return;
        }


        // Open context
        _this->ctx = iio_create_context_from_uri(_this->uri);
        if (_this->ctx == NULL) {
            flog::error("Could not open avc ({})", _this->uri);
            return;
        }

        // Get phy and device handle
        _this->phy = iio_context_find_device(_this->ctx, "rfic-phy");
        if (_this->phy == NULL) {
            flog::error("Could not connect to avc phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "avc_rxstream");
        if (_this->dev == NULL) {
            flog::error("Could not connect to avc dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Use SoftRetune
        iio_device_debug_attr_write(_this->phy, "api", "tsrfpllhw 2 0");
        // iio_device_reg_write(_this->dev, 1, 1);
        iio_device_debug_attr_write(_this->phy, "api", "tsdcoc 1 1");
        iio_device_debug_attr_write(_this->phy, "api", "tsrxcal 1");
        iio_device_reg_write(_this->phy, 0x040, 0x0f); // enble gpio
        iio_device_reg_write(_this->phy, 0x020, 0x08); // disable iq swap

        {
            uint32_t val;
            iio_device_reg_read(_this->phy, 0x047, &val);
            if (_this->enable_lna)
                iio_device_reg_write(_this->phy, 0x047, val | (1 << 6));
            else
                iio_device_reg_write(_this->phy, 0x047, val & (~(1 << 6)));
        }

        // Get RX channels
        _this->rxChan = iio_device_find_channel(_this->phy, "voltage2", false);
        _this->rxLO = iio_device_find_channel(_this->phy, "altvoltage6", true);


        iio_device_attr_write_longlong(_this->phy, "sampling_frequency", round(_this->samplerate)); // Sample rate
        iio_channel_attr_write_longlong(_this->rxChan, "bandwidth", _this->bandwidth);

        {
            int idx = round(_this->gain + 2.0);
            iio_device_reg_write(_this->phy, 0x378, full_gain[idx] & 0xff);
            iio_device_reg_write(_this->phy, 0x379, (full_gain[idx] >> 8) & 0x3);
        }

        // iio_channel_attr_write(_this->rxChan, "rfport", _this->RFPort.value(_this->RFPortId).c_str());
        {
            uint32_t val;
            uint8_t chn = 0x3; //todo: from linux api
            iio_device_reg_read(_this->dev, 0x0, &val);
            val = val & ~(0x3 << 2);

            if (chn == 0xf) {
                iio_device_reg_write(_this->dev, 0x0, val | (0x3 << 2));
            }
            else if (chn == 0x3)
                iio_device_reg_write(_this->dev, 0x0, val | (0x1 << 2));
            else if (chn == 0xc)
                iio_device_reg_write(_this->dev, 0x0, val | (0x2 << 2));
        }
        // Start worker thread
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        flog::info("AvcSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        if (!_this->running) {
            return;
        }

        // Stop worker thread
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // Close device
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        flog::info("AvcSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // Tune device
            iio_channel_attr_write_longlong(_this->rxLO, "frequency", round(freq));
        }
        flog::info("AvcSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;

        if (_this->running) {
            SmGui::BeginDisabled();
        }
        SmGui::FillWidth();
        SmGui::ForceSync();
        ImGui::LeftLabel("IP");
        if (ImGui::InputText(CONCAT("##_avcsdr_ip_", _this->name), _this->uri, 20)) {
            config.acquire();
            config.conf["IP"] = std::string(_this->uri);
            config.release(true);
        }

        SmGui::LeftLabel("SampleRate");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_avcsdr_sr_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->samplerate = _this->samplerates.value(_this->srId);
            config.acquire();
            config.conf["devices"]["samplerate"] = _this->samplerate;
            config.release(true);
            core::setInputSampleRate(_this->samplerate);
        }

        if (_this->running) {
            SmGui::EndDisabled();
        }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_avcsdr_bw_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            _this->bandwidth = _this->bandwidths.value(_this->bwId);
            if (_this->running) {
                iio_channel_attr_write_longlong(_this->rxChan, "bandwidth", _this->bandwidth);
            }

            config.acquire();
            config.conf["devices"]["bandwidth"] = _this->bandwidth;
            config.release(true);
        }

        // SmGui::LeftLabel("RF Port");
        // SmGui::FillWidth();
        // SmGui::ForceSync();
        // if (SmGui::Combo(CONCAT("##_avc_port_select_", _this->name), &_this->RFPortId, _this->RFPort.txt))
        // {
        //     if (_this->running)
        //     {
        //         iio_channel_attr_write(_this->rxChan, "rfport", _this->RFPort.value(_this->RFPortId).c_str());
        //     }

        //     config.acquire();
        //     config.conf["devices"]["rfport"] = _this->RFPort.key(_this->RFPortId);
        //     config.release(true);
        // }

        SmGui::LeftLabel("internel Gain");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_avc_gain__", _this->name), &_this->gain, -2.0f, 70.0f, 1.0f, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                int idx = round(_this->gain + 2.0);
                iio_device_reg_write(_this->phy, 0x378, full_gain[idx] & 0xff);
                iio_device_reg_write(_this->phy, 0x379, (full_gain[idx] >> 8) & 0x3);
            }

            config.acquire();
            config.conf["devices"]["gain"] = _this->gain;
            config.release(true);
        }

        if (SmGui::Checkbox("Enable exLNA", &_this->enable_lna)) {
            if (_this->running) {
                uint32_t val;
                iio_device_reg_read(_this->phy, 0x047, &val);
                if (_this->enable_lna)
                    iio_device_reg_write(_this->phy, 0x047, val | (1 << 6));
                else
                    iio_device_reg_write(_this->phy, 0x047, val & (~(1 << 6)));
            }
            config.acquire();
            config.conf["devices"]["enable_lna"] = _this->enable_lna;
            config.release(true);
        }

        if (SmGui::Button("Hw Rx Calibration")) {
            if (_this->running) {
                iio_device_debug_attr_write(_this->phy, "api", "tsdcoc 1 1");
                iio_device_debug_attr_write(_this->phy, "api", "tsrxcal 1");
            }
        }
        // if (_this->RFPortId) { SmGui::EndDisabled(); }
    }

    void setBandwidth(int bw) {
        // if (bw > 0) {
        //     iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", bw);
        // }
        // else {
        //     iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", std::min<int>(samplerate, 52000000));
        // }
    }

    static void worker(void* ctx) {

        AvcSDRSourceModule* _this = (AvcSDRSourceModule*)ctx;
        int blockSize = _this->samplerate / 200.0f;

        // Acquire channels
        iio_channel* rx0_i = iio_device_find_channel(_this->dev, "voltage0", 0);
        iio_channel* rx0_q = iio_device_find_channel(_this->dev, "voltage1", 0);
        if (!rx0_i || !rx0_q) {
            flog::error("Failed to acquire RX channels");
            return;
        }

        // Start streaming
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);

        // Allocate buffer
        iio_buffer* rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            flog::error("Could not create RX buffer");
            return;
        }

        // Receive loop
        while (true) {
            // Read samples
            iio_buffer_refill(rxbuf);

            // Get buffer pointer
            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);
            if (!buf) {
                break;
            }

            // Convert samples to CF32
            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buf, 32768.0f, blockSize * 2);

            // Send out the samples
            if (!_this->stream.swap(blockSize)) {
                break;
            };
        }

        // Stop streaming
        iio_channel_disable(rx0_i);
        iio_channel_disable(rx0_q);

        // Free buffer
        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    iio_context* ctx = NULL;
    iio_device* phy = NULL;
    iio_device* dev = NULL;
    iio_channel* rxLO = NULL;
    iio_channel* rxChan = NULL;
    bool running = false;

    std::string devDesc = "";
    char uri[1024] = "ip:192.168.50.11";

    double freq;
    int samplerate = 1000000;
    int bandwidth = 0;
    float gain = -1;
    bool enable_lna = false;

    int devId = 0;
    int srId = 0;
    int bwId = 0;
    int RFPortId = 0;

    OptionList<std::string, std::string> devices;
    OptionList<int, double> samplerates;
    OptionList<int, double> bandwidths;
    OptionList<std::string, std::string> RFPort;
};

MOD_EXPORT void _INIT_() {
    json defConf = {};
    defConf["devices"] = {};
    config.setPath(core::args["root"].s() + "/avcsdr_source_config.json");
    config.load(defConf);
    config.enableAutoSave();

    // Reset the configuration if the old format is still used
    config.acquire();
    if (!config.conf.contains("devices")) {
        config.conf = defConf;
        config.release(true);
    }
    else {
        config.release();
    }
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AvcSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AvcSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}