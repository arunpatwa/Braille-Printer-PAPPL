#include <pappl/pappl.h>
#include <cupsfilters/filter.h>
#include <ppd/ppd-filter.h>

typedef struct brf_spooling_conversion_s
{
    char *srctype;                         // Input data type
    char *dsttype;                         // Output data type
    int num_filters;                       // Number of filters
    cf_filter_filter_in_chain_t filters[]; // List of filters with
                                           // parameters
} brf_spooling_conversion_t;

typedef struct brf_printer_app_config_s
{
  // Identification of the Printer Application
  const char *system_name;           // Name of the system
  const char *system_package_name;   // Name of Printer Application
                                     // package/executable
  const char *version;               // Program version number string
  unsigned short numeric_version[4]; // Numeric program version
  const char *web_if_footer;         // HTML Footer for web interface

  pappl_pr_autoadd_cb_t autoadd_cb;

  pappl_pr_identify_cb_t identify_cb;

  pappl_pr_testpage_cb_t testpage_cb;

  cups_array_t *spooling_conversions;

  cups_array_t *stream_formats;
  const char *backends_ignore;

  const char *backends_only;

  void *testpage_data;

} brf_printer_app_config_t;


typedef struct brf_printer_app_global_data_s
{
  brf_printer_app_config_t *config;
  pappl_system_t *system;
  int num_drivers;            // Number of drivers (from the PPDs)
  pappl_pr_driver_t *drivers; // Driver index (for menu and
                              // auto-add)
  char spool_dir[1024];       // Spool directory, customizable via
                              // SPOOL_DIR environment variable

} brf_printer_app_global_data_t;

// Data for brf_print_filter_function()
typedef struct brf_print_filter_function_data_s
// look-up table
{
  pappl_device_t *device;                     // Device
  const char *device_uri;                     // Printer device URI
  pappl_job_t *job;                           // Job
  brf_printer_app_global_data_t *global_data; // Global data
} brf_print_filter_function_data_t;

typedef struct brf_cups_device_data_s
{
  const char *device_uri; // Device URI
  int inputfd,            // FD for job data input
      backfd,             // FD for back channel
      sidefd;             // FD for side channel
  int backend_pid;        // PID of CUPS backend
  double back_timeout,    // Timeout back channel (sec)
      side_timeout;       // Timeout side channel (sec)

  cf_filter_filter_in_chain_t *chain_filter; // Filter from PPD file
  cf_filter_data_t *filter_data;             // Common data for filter functions
  cf_filter_external_t backend_params;       // Parameters for launching
                                             // backend via ppdFilterExternalCUPS()
  bool internal_filter_data;                 // Is filter_data
                                             // internal?
} brf_cups_device_data_t;

char *filter_envp[] = {"PPD=/home/arun/open-printing/Braille-printer-app/BRF.ppd", "CONTENT_TYPE=text/plain", NULL};

static cf_filter_external_t texttobrf_filter = {

    .filter = "/usr/lib/cups/filter/texttobrf",
    .envp = filter_envp,

};

static cf_filter_external_t brftopagedbrf_filter = {

    .filter = "/usr/lib/cups/filter/brftopagedbrf",
    .envp = filter_envp,

};

static cf_filter_external_t imagetobrf_filter = {

    .filter = "/usr/lib/cups/filter/imagetobrf",
    .envp = filter_envp,

};

static cf_filter_external_t imagetoubrl_filter = {

    .filter = "/usr/lib/cups/filter/imagetoubrl",
    .envp = filter_envp,

};


static cf_filter_external_t vectortobrf_filter = {

    .filter = "/usr/lib/cups/filter/vectortobrf",
    .envp = filter_envp,

};


static cf_filter_external_t vectortoubrl_filter = {

    .filter = "/usr/lib/cups/filter/vectortoubrl",
    .envp = filter_envp,

};


static brf_spooling_conversion_t brf_convert_pdf_to_brf =
{
    "application/pdf",
    "application/vnd.cups-brf",
    1,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};


static brf_spooling_conversion_t brf_convert_text_to_brf =
{
    "text/plain",
    "application/vnd.cups-brf",
    1,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};


static brf_spooling_conversion_t brf_convert_html_to_brf =
{
    "text/html",
    "application/vnd.cups-brf",
    1,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_brf_to_paged_brf =
{
    "application/vnd.cups-brf",
    "application/vnd.cups-paged-brf",
    0,
    {
        {cfFilterExternal,
        &brftopagedbrf_filter,
        "brftopagedbrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_ubrl_to_paged_ubrl =
{
    "application/vnd.cups-ubrl",
    "application/vnd.cups-paged-ubrl",
    0,
    {
        {cfFilterExternal,
        &brftopagedbrf_filter,
        "brftopagedbrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_doc_to_brf =
{
    "application/msword",
    "application/vnd.cups-brf",
    30,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_rtf_text_to_brf =
{
    "text/rtf",
    "application/vnd.cups-brf",
    30,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_rtf_app_to_brf =
{
    "application/rtf",
    "application/vnd.cups-brf",
    30,
    {
        {cfFilterExternal,
        &texttobrf_filter,
        "texttobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_jpeg_to_brf =
{
    "image/jpeg",
    "image/vnd.cups-brf",
    70,
    {
        {cfFilterExternal,
        &imagetobrf_filter,
        "imagetobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_png_to_brf =
{
    "image/png",
    "image/vnd.cups-brf",
    70,
    {
        {cfFilterExternal,
        &imagetobrf_filter,
        "imagetobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_jpeg_to_ubrl =
{
    "image/jpeg",
    "image/vnd.cups-ubrl",
    70,
    {
        {cfFilterExternal,
        &imagetoubrl_filter,
        "imagetoubrl"
        }
    }
};

static brf_spooling_conversion_t brf_convert_png_to_ubrl =
{
    "image/png",
    "image/vnd.cups-ubrl",
    70,
    {
        {cfFilterExternal,
        &imagetoubrl_filter,
        "imagetoubrl"
        }
    }
};

static brf_spooling_conversion_t brf_convert_vector_to_brf =
{
    "image/vnd.cups-pdf",
    "image/vnd.cups-brf",
    30,
    {
        {cfFilterExternal,
        &vectortobrf_filter,
        "vectortobrf"
        }
    }
};

static brf_spooling_conversion_t brf_convert_vector_to_ubrl =
{
    "image/vnd.cups-pdf",
    "image/vnd.cups-ubrl",
    30,
    {
        {cfFilterExternal,
        &vectortoubrl_filter,
        "vectortoubrl"
        }
    }
};