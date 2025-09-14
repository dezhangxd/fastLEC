#pragma once

#include <string>
#include <cstdio>
#include <stdexcept>
#include <type_traits>
#include "basic.hpp"

namespace fastLEC
{
// X-Macro defines all modes
#define MODES_LIST   \
    X(ES)            \
    X(BDD)           \
    X(pES)           \
    X(SAT)           \
    X(pSAT)          \
    X(gpuES)         \
    X(SAT_sweeping)  \
    X(ES_sweeping)   \
    X(pES_sweeping)  \
    X(pSAT_sweeping) \
    X(dp2_sweeping)

// User-defined parameters macro with descriptions
#define USER_PARAMS                                                                         \
    USER_PARAM(learning_rate, double, 0.001, "Learning rate for optimization")              \
    USER_PARAM(max_iterations, int, 1000, "Maximum number of iterations")                   \
    USER_PARAM(epsilon, double, 1e-6, "Convergence threshold")                              \
    USER_PARAM(use_gpu, bool, false, "Enable GPU acceleration")                             \
    USER_PARAM(ls_bv_bits, int, 17, "bitvector width in log scale for logic synthesis")     \
    USER_PARAM(es_bv_bits, int, 14, "bitvector width in log scale for para/seq simulation") \
    USER_PARAM(use_ies, bool, true, "Enable iES")                                           \
    USER_PARAM(use_pes_pbit, bool, false, "Enable para-bits for para-es")                   \
    USER_PARAM(ies_u64, bool, false, "Enable iES with u64_int, default using long BV for ies")                             \
    USER_PARAM(seed, int, 0, "Random seed for reproducibility")

    // Custom parameters structure (auto-generated)
    struct CustomParams
    {
#define USER_PARAM(name, type, default_val, description) type name = default_val;
        USER_PARAMS
#undef USER_PARAM
    };

    enum Mode
    {
#define X(name) name,
        MODES_LIST
#undef X
    };

    class Param
    {
    private:
        Param() = default;
        Param(const Param &) = delete;
        Param &operator=(const Param &) = delete;

    public:
        static Param &get()
        {
            static Param instance;
            return instance;
        }

        // parameters
        std::string input_file;
        Mode mode;
        unsigned n_threads;
        unsigned verbose;
        double timeout;
        CustomParams custom_params;

        void init()
        {
            input_file = "";
            mode = Mode::ES;
            n_threads = 1;
            verbose = 1;
            timeout = 3600.0;
            custom_params = CustomParams{};
        }

        void setCustomParam(const std::string &name, const std::string &value)
        {
#define USER_PARAM(param_name, type, default_val, description)            \
    if (name == #param_name)                                              \
    {                                                                     \
        if constexpr (std::is_same<type, bool>::value)                         \
        {                                                                 \
            custom_params.param_name = (value == "true" || value == "1"); \
        }                                                                 \
        else if constexpr (std::is_same<type, int>::value)                     \
        {                                                                 \
            custom_params.param_name = std::stoi(value);                  \
        }                                                                 \
        else if constexpr (std::is_same<type, double>::value)                  \
        {                                                                 \
            custom_params.param_name = std::stod(value);                  \
        }                                                                 \
        return;                                                           \
    }
            USER_PARAMS
#undef USER_PARAM

            fprintf(stderr, "c [parser] error: unknown custom parameter: %s\n", name.c_str());
            help(2);
            exit(1);
        }

        template <typename T>
        T getCustomParam(const std::string &name) const
        {
#define USER_PARAM(param_name, type, default_val, description) \
    if (name == #param_name)                                   \
        return custom_params.param_name;
            USER_PARAMS
#undef USER_PARAM

            throw std::runtime_error("Parameter not found: " + name);
        }

        void help(unsigned show = 3)
        {
            if (show == 1 || show == 3)
            {
                printf("c fastLEC: A Fast and Scalable LEC prover for AIGER Circuits (version 0.1)\n");
                printf("c Usage: fastLEC -i <input.aig> [options]\n");
                printf("c Options:\n");
                printf("c   -h, --help                show this help information\n");
                printf("c   --modes                   show available modes\n");
                printf("c   -m, --mode <mode>         set the mode [default: ES]\n");
                printf("c   -i, --input <filename>    set the input file\n");
                printf("c   -c, --cores <num>         set the number of threads [default: 1]\n");
                printf("c   -t, --timeout <time>      set the timeout [default: 30.0 seconds]\n");
                printf("c   -v, --verbose <level>     set verbose level [default: 1]\n");
                printf("c   -p, --param <key> <val>   set custom parameter\n");
            }
            if (show == 2 || show == 3)
            {
                printf("c Custom Parameters:\n");
#define USER_PARAM(param_name, type, default_val, description)     \
    printf("c     %-20s %s [default: ", #param_name, description); \
    if constexpr (std::is_same<type, bool>::value)                      \
    {                                                              \
        printf("%s", static_cast<bool>(default_val) ? "true" : "false"); \
    }                                                              \
    else if constexpr (std::is_same<type, int>::value)                  \
    {                                                              \
        printf("%d", static_cast<int>(default_val));               \
    }                                                              \
    else if constexpr (std::is_same<type, double>::value)               \
    {                                                              \
        printf("%g", static_cast<double>(default_val));            \
    }                                                              \
    printf("]\n");
                USER_PARAMS
#undef USER_PARAM
            }
            exit(0);
        }

        void prt_modes()
        {
            printf("c Available modes:\n");
#define X(name) printf("c   %s\n", #name);
            MODES_LIST
#undef X
            exit(0);
        }

        bool parse(int argc, char **argv)
        {
            Param &p = get();
            p.init();

            for (int i = 1; i < argc; ++i)
            {
                std::string arg = std::string(argv[i]);
                if (arg == "-h" || arg == "--help")
                {
                    p.help(1);
                }
                else if (arg == "--modes")
                {
                    p.prt_modes();
                }
                else if (arg == "-c" || arg == "--cores")
                {
                    p.n_threads = atoi(argv[++i]);
                }
                else if (arg == "-t" || arg == "--timeout")
                {
                    p.timeout = atof(argv[++i]);
                }
                else if (arg == "-i" || arg == "--input")
                {
                    p.input_file = std::string(argv[++i]);
                }
                else if (arg == "-v" || arg == "--verbose")
                {
                    p.verbose = atoi(argv[++i]);
                }
                else if (arg == "-m" || arg == "--mode")
                {
                    std::string mode_str = std::string(argv[++i]);

                    bool mode_found = false;
#define X(name)              \
    if (mode_str == #name)   \
    {                        \
        p.mode = Mode::name; \
        mode_found = true;   \
    }
                    MODES_LIST
#undef X

                    if (!mode_found)
                    {
                        fprintf(stderr, "c [parser] error: invalid mode: %s\n", mode_str.c_str());
                        p.prt_modes();
                        return false;
                    }
                }
                else if (arg == "-p" || arg == "--param")
                {
                    if (i + 2 >= argc)
                    {
                        fprintf(stderr, "c [parser] error: -p requires two arguments: <key> <value>\n");
                        p.help(2);
                        return false;
                    }
                    std::string key = std::string(argv[++i]);
                    std::string value = std::string(argv[++i]);
                    p.setCustomParam(key, value);
                }
                else
                {
                    fprintf(stderr, "c [parser] error: unknown option: %s\n", argv[i]);
                    p.help(1);
                    return false;
                }
            }

            if (p.input_file.empty())
            {
                fprintf(stderr, "c [parser] error: input file is not set\n");
                p.help(1);
                return false;
            }

            fastLEC::ResMgr::get().init();
            fastLEC::ResMgr::get().set_seed(p.getCustomParam<int>("seed"));

            if (p.verbose > 0)
            {
                printf("c ------------------------------------------------------------\n");
                printf("c fastLEC v1.0\n");
                printf("c Author: Xindi Zhang\n");
                printf("c Email: dezhangxd96@gmail.com\n");
                printf("c ------------------------------------------------------------\n");
                printf("c [parser] input file: %s\n", p.input_file.c_str());

                // Print mode
                std::string mode_str;
#define X(name)               \
    if (p.mode == Mode::name) \
        mode_str = #name;
                MODES_LIST
#undef X
                printf("c [parser] mode: %s\n", mode_str.c_str());

                // Print basic parameters
                printf("c [parser] threads: %u\n", p.n_threads);
                printf("c [parser] timeout: %.2f seconds\n", p.timeout);
                printf("c [parser] verbose: %u\n", p.verbose);

                // Print custom parameters that differ from defaults
                // printf("c [parser] custom parameters:\n");
#define USER_PARAM(param_name, type, default_val, description)                                                \
    if (p.custom_params.param_name != default_val)                                                            \
    {                                                                                                         \
        if constexpr (std::is_same<type, bool>::value)                                                             \
        {                                                                                                     \
            printf("c [parser] usr:%s = %s\n", #param_name, p.custom_params.param_name ? "true" : "false");   \
        }                                                                                                     \
        else if constexpr (std::is_same<type, int>::value)                                                         \
        {                                                                                                     \
            printf("c [parser] usr:%s = %d\n", #param_name, static_cast<int>(p.custom_params.param_name));    \
        }                                                                                                     \
        else if constexpr (std::is_same<type, double>::value)                                                      \
        {                                                                                                     \
            printf("c [parser] usr:%s = %g\n", #param_name, static_cast<double>(p.custom_params.param_name)); \
        }                                                                                                     \
    }
                USER_PARAMS
#undef USER_PARAM

                printf("c ------------------------------------------------------------\n");
                fflush(stdout);
            }

            return true;
        }
    };

}
