// Include necessary headers

#include <strings.h>
#include <cupsfilters/log.h>
#include <cupsfilters/filter.h>
#include <ppd/ppd-filter.h>
#include <limits.h>
#include <pappl/pappl.h>
#include <liblouisutdml/liblouisutdml.h>
#include <cups/backend.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Include necessary headers...

#include <pappl/pappl.h>
#include <math.h>

#define brf_TESTPAGE_MIMETYPE "application/vnd.cups-brf";

// Local functions...

static bool brf_gen_printfile(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool brf_gen_rendjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool brf_gen_rendpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool brf_gen_rstartjob(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device);
static bool brf_gen_rstartpage(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned page);
static bool brf_gen_status(pappl_printer_t *printer);
static bool brf_gen_rwriteline(pappl_job_t *job, pappl_pr_options_t *options, pappl_device_t *device, unsigned y, const unsigned char *line);

static const char *const brf_gen_media[] =
    { // Supported media sizes for Generic BRF printers
        "na_legal_8.5x14in",
        "na_letter_8.5x11in",
        "na_executive_7x10in",
        "iso_a4_210x297mm",
        "iso_a5_148x210mm",
        "jis_b5_182x257mm",
        "iso_b5_176x250mm",
        "na_number-10_4.125x9.5in",
        "iso_c5_162x229mm",
        "iso_dl_110x220mm",
        "na_monarch_3.875x7.5in"};

bool // O - `true` on success, `false` on error
brf_gen(
    pappl_system_t *system,              // I - System
    const char *driver_name,             // I - Driver name
    const char *device_uri,              // I - Device URI
    const char *device_id,               // I - 1284 device ID
    pappl_pr_driver_data_t *driver_data, // I - Pointer to driver data
    ipp_t **attrs,                       // O - Pointer to driver attributes
    void *cbdata)                        // I - Callback data (not used)
{
  driver_data->printfile_cb = brf_gen_printfile;
  driver_data->rendjob_cb = brf_gen_rendjob;
  driver_data->rendpage_cb = brf_gen_rendpage;
  driver_data->rstartjob_cb = brf_gen_rstartjob;
  driver_data->rstartpage_cb = brf_gen_rstartpage;
  driver_data->rwriteline_cb = brf_gen_rwriteline;
  driver_data->status_cb = brf_gen_status;
  driver_data->format = brf_TESTPAGE_MIMETYPE;

  driver_data->num_resolution = 1;
  driver_data->x_resolution[0] = 200;
  driver_data->y_resolution[0] = 200;
  // driver_data->x_resolution[1] = 300;
  // driver_data->y_resolution[1] = 300;

  driver_data->x_default = driver_data->y_default = driver_data->x_resolution[0];

  driver_data->num_media = (int)(sizeof(brf_gen_media) / sizeof(brf_gen_media[0]));
  memcpy(driver_data->media, brf_gen_media, sizeof(brf_gen_media));

  papplCopyString(driver_data->media_default.size_name, "iso_a4_210x297mm", sizeof(driver_data->media_default.size_name));
  driver_data->media_default.size_width = 1 * 21000;
  driver_data->media_default.size_length = 1 * 29700;
  driver_data->left_right = 635; // 1/4" left and right
  driver_data->bottom_top = 1270;


  driver_data->media_default.bottom_margin = driver_data->bottom_top;
  driver_data->media_default.left_margin = driver_data->left_right;
  driver_data->media_default.right_margin = driver_data->left_right;
  driver_data->media_default.top_margin = driver_data->bottom_top;
  /* Three paper trays (MSN names) */
  driver_data->num_source = 3;
  driver_data->source[0] = "tray-1";
  driver_data->source[1] = "manual";
  driver_data->source[2] = "envelope";
  // a types (MSN names) */
  driver_data->num_type = 8;
  driver_data->type[0] = "stationery";
  driver_data->type[1] = "stationery-inkjet";
  driver_data->type[2] = "stationery-letterhead";
  driver_data->type[3] = "cardstock";
  driver_data->type[4] = "labels";
  driver_data->type[5] = "envelope";
  driver_data->type[6] = "transparency";
  driver_data->type[7] = "photographic";
  papplCopyString(driver_data->media_default.source, "tray-1", sizeof(driver_data->media_default.source));
  papplCopyString(driver_data->media_default.type, "labels", sizeof(driver_data->media_default.type));
  driver_data->media_ready[0] = driver_data->media_default;

  printf("************************gen-brf-called *********************************\n");

  return (true);
}

// 'Brf_generic_print()' - Print a file.

static bool // O - `true` on success, `false` on failure
brf_gen_printfile(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device)      // I - Output device
{
  int fd;             // Input file
  ssize_t bytes;      // Bytes read/written
  char buffer[65536]; // Read/write buffer

  // Copy the raw file...
  papplJobSetImpressions(job, 1);

  if ((fd = open(papplJobGetFilename(job), O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open print file '%s': %s", papplJobGetFilename(job), strerror(errno));
    return (false);
  }

  while ((bytes = read(fd, buffer, sizeof(buffer))) > 0)
  {
    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to send %d bytes to printer.", (int)bytes);
      close(fd);
      return (false);
    }
  }
  close(fd);

  papplJobSetImpressionsCompleted(job, 1);

  return (true);
}

// 'Brf_generic_rendjob()' - End a job.

static bool // O - `true` on success, `false` on failure
brf_gen_rendjob(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device)      // I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}

// 'Brf_generic_rendpage()' - End a page.

static bool // O - `true` on success, `false` on failure
brf_gen_rendpage(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device,      // I - Output device
    unsigned page)               // I - Page number
{
  (void)job;
  (void)page;

  papplDevicePuts(device, "P1\n");

  return (true);
}

// 'Brf_generic_rstartjob()' - Start a job.

static bool // O - `true` on success, `false` on failure
brf_gen_rstartjob(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device)      // I - Output device
{
  (void)job;
  (void)options;
  (void)device;

  return (true);
}

// 'brf_gen_rwriteline()' - Write a raster line.

static bool // O - `true` on success, `false` on failure
brf_gen_rwriteline(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device,      // I - Output device
    unsigned y,                  // I - Line number
    const unsigned char *line)   // I - Line
{
  if (line[0] || memcmp(line, line + 1, options->header.cupsBytesPerLine - 1))
  {
    unsigned i;                   // Looping var
    const unsigned char *lineptr; // Pointer into line
    unsigned char buffer[300],
        *bufptr; // Pointer into buffer

    for (i = options->header.cupsBytesPerLine, lineptr = line, bufptr = buffer; i > 0; i--)
      *bufptr++ = ~*lineptr++;

    papplDevicePrintf(device, "GW0,%u,%u,1\n", y, options->header.cupsBytesPerLine);
    papplDeviceWrite(device, buffer, options->header.cupsBytesPerLine);
    papplDevicePuts(device, "\n");
  }

  return (true);
}

// 'Brf_generic_rstartpage()' - Start a page.

static bool // O - `true` on success, `false` on failure
brf_gen_rstartpage(
    pappl_job_t *job,            // I - Job
    pappl_pr_options_t *options, // I - Job options
    pappl_device_t *device,      // I - Output device
    unsigned page)               // I - Page number
{
  int ips; // Inches per second

  (void)job;
  (void)page;

  papplDevicePuts(device, "\nN\n");

  return (true);
}

// 'Brf_generic_status()' - Get current printer status.

static bool // O - `true` on success, `false` on failure
brf_gen_status(
    pappl_printer_t *printer) // I - Printer
{
  (void)printer;

  return (true);
}


#define brf_TESTPAGE_HEADER "T*E*S*T*P*A*G*E*"
#define brf_TESTPAGE_MIMETYPE "application/vnd.cups-brf"

extern bool brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern char *strdup(const char *);
//
// Local functions...
//
static bool BRFTestFilterCB(pappl_job_t *job, pappl_device_t *device, void *cbdata);

static int brf_print_filter_function(int inputfd, int outputfd, int inputseekable, cf_filter_data_t *data, void *parameters);

static const char *autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata);

static bool driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);

static int match_id(int num_did, cups_option_t *did, const char *match_id);

static const char *mime_cb(const unsigned char *header, size_t headersize, void *data);

static bool printer_cb(const char *device_info, const char *device_uri, const char *device_id, pappl_system_t *system);

static pappl_system_t *system_cb(int num_options, cups_option_t *options, void *data);

// Local globals...

static pappl_pr_driver_t brf_drivers[] =
    {
        // Driver list
        {"gen_brf", "Generic Braille embosser",
         NULL, NULL},

};

// State file

static char brf_statefile[1024];


// 'main()' - Main entry for brf.


int                // O - Exit status
main(int argc,     // I - Number of command-line arguments
     char *argv[]) // I - Command-line arguments
{
  return (papplMainloop(argc, argv,
                        "1.0",
                        "Arun Patwa 2024",
                        (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])),
                        brf_drivers, autoadd_cb, driver_cb,
                        /*subcmd_name*/ NULL, /*subcmd_cb*/ NULL,
                        system_cb,
                        /*usage_cb*/ NULL,
                        /*data*/ NULL));
}

// 'autoadd_cb()' - Determine the proper driver for a given printer.

static const char *                 // O - Driver name or `NULL` for none
autoadd_cb(const char *device_info, // I - Device information/name (not used)
           const char *device_uri,  // I - Device URI
           const char *device_id,   // I - IEEE-1284 device ID
           void *cbdata)            // I - Callback data (System)
{
  int i,                 // Looping var
      score,             // Current driver match score
      best_score = 0,    // Best score
      num_did;           // Number of device ID key/value pairs
  cups_option_t *did;    // Device ID key/value pairs
  const char *make,      // Manufacturer name
      *best_name = NULL; // Best driver

  (void)device_info;



  // First parse the device ID and get any potential driver name to match


  num_did = papplDeviceParseID(device_id, &did);

  if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
    if ((make = cupsGetOption("MANU", num_did, did)) == NULL)
      make = cupsGetOption("MFG", num_did, did);

  // Then loop through the driver list to find the best match...
  // for (i = 0; i < (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])); i ++)
  {

    if (brf_drivers[i].device_id)
    {
      // See if we have a matching device ID...
      score = match_id(num_did, did, brf_drivers[i].device_id);
      if (score > best_score)
      {
        best_score = score;
        best_name = brf_drivers[i].name;
      }
    }
  }

  // Clean up and return...
  cupsFreeOptions(num_did, did);

  return (best_name);
}

// 'match_id()' - Compare two IEEE-1284 device IDs and return a score.

// The score is 2 for each exact match and 1 for a partial match in a comma

// delimited field.  Any non-match results in a score of 0.


static int                     // O - Score
match_id(int num_did,          // I - Number of device ID key/value pairs
         cups_option_t *did,   // I - Device ID key/value pairs
         const char *match_id) // I - Driver's device ID match string
{
  int i,              // Looping var
      score = 0,      // Score
      num_mid;        // Number of match ID key/value pairs
  cups_option_t *mid, // Match ID key/value pairs
      *current;       // Current key/value pair
  const char *value,  // Device ID value
      *valptr;        // Pointer into value

  // Parse the matching device ID into key/value pairs...
  if ((num_mid = papplDeviceParseID(match_id, &mid)) == 0)
    return (0);

  // Loop through the match pairs to find matches (or not)
  for (i = num_mid, current = mid; i > 0; i--, current++)
  {
    if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
    {
      // No match
      score = 0;
      break;
    }

    if (!strcasecmp(current->value, value))
    {
      // Full match!
      score += 2;
    }
    else if ((valptr = strstr(value, current->value)) != NULL)
    {
      // Possible substring match, check
      size_t mlen = strlen(current->value);
      // Length of match value
      if ((valptr == value || valptr[-1] == ',') && (!valptr[mlen] || valptr[mlen] == ','))
      {
        // Partial match!
        score++;
      }
      else
      {
        // No match
        score = 0;
        break;
      }
    }
    else
    {
      // No match
      score = 0;
      break;
    }
  }
}


// 'driver_cb()' - Main driver callback



static bool // O - `true` on success, `false` on error
driver_cb(
    pappl_system_t *system,       // I - System
    const char *driver_name,      // I - Driver name
    const char *device_uri,       // I - Device URI
    const char *device_id,        // I - 1284 device ID
    pappl_pr_driver_data_t *data, // I - Pointer to driver data
    ipp_t **attrs,                // O - Pointer to driver attributes
    void *cbdata)                 // I - Callback data (not used)
{
  int i; // Looping var

  // Copy make/model info...

  for (i = 0; i < (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])); i++)
  {
    if (!strcmp(driver_name, brf_drivers[i].name))
    {
      papplCopyString(data->make_and_model, brf_drivers[i].description, sizeof(data->make_and_model));
      break;
    }
  }

  // Pages per minute
  data->ppm = 1;

  // Color values...
  data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
  data->color_default = PAPPL_COLOR_MODE_MONOCHROME;
  data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_8; // to be done just guess

  // "print-quality-default" value...
  data->quality_default = IPP_QUALITY_NORMAL;
  data->orient_default = IPP_ORIENT_NONE;

  // "sides" values...
  data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  data->sides_default = PAPPL_SIDES_ONE_SIDED;

  // "orientation-requested-default" value...
  data->orient_default = IPP_ORIENT_NONE;

  // Test page callback

  // data->testpage_cb = lprintTestPageCB;

  // Use the corresponding sub-driver callback to set things up...
  if (!strncmp(driver_name, "gen_", 4))
    return (brf_gen(system, driver_name, device_uri, device_id, data, attrs, cbdata));

  else{
    printf("****************brfgen-not called***************\n");
    return (false);
  }
}

// 'mime_cb()' - MIME typing callback...

static const char *                  // O - MIME media type or `NULL` if none
mime_cb(const unsigned char *header, // I - Header data
        size_t headersize,           // I - Size of header data
        void *cbdata)                // I - Callback data (not used)
{

  return (brf_TESTPAGE_MIMETYPE);
}

// 'printer_cb()' - Try auto-adding printers.

static bool                         // O - `false` to continue
printer_cb(const char *device_info, // I - Device information
           const char *device_uri,  // I - Device URI
           const char *device_id,   // I - IEEE-1284 device ID
           pappl_system_t *system)  // I - System
{
  const char *driver_name = autoadd_cb(device_info, device_uri, device_id, system);

  // Driver name, if any

  if (driver_name)
  {
    char name[128], // Printer name
        *nameptr;   // Pointer in name
    papplCopyString(name, device_info, sizeof(name));

    if ((nameptr = strstr(name, " (")) != NULL)
      *nameptr = '\0';

    if (!papplPrinterCreate(system, 0, name, driver_name, device_id, device_uri))
    {
      // Printer already exists with this name, so try adding a number to the name
      int i;                         // Looping var
      char newname[128],             // New name
          number[4];                 // Number string
      size_t namelen = strlen(name), // Length of original name string
          numberlen;                 // Length of number string

      for (i = 2; i < 100; i++)
      {
        // Append " NNN" to the name, truncating the existing name as needed to
        // include the number at the end...

        snprintf(number, sizeof(number), " %d", i);
        numberlen = strlen(number);

        papplCopyString(newname, name, sizeof(newname));
        if ((namelen + numberlen) < sizeof(newname))
          memcpy(newname + namelen, number, numberlen + 1);
        else
          memcpy(newname + sizeof(newname) - numberlen - 1, number, numberlen + 1);

        // Try creating with this name...

        if (papplPrinterCreate(system, 0, newname, driver_name, device_id, device_uri))
          break;
      }
    }
  }

  return (false);
}

// 'system_cb()' - Setup the system object.


static pappl_system_t * // O - System object
system_cb(
    int num_options,        // I - Number options
    cups_option_t *options, // I - Options
    void *data)             // I - Callback data (unused)
{
  pappl_system_t *system;    // System object
  const char *val,           // Current option value
      *hostname,             // Hostname, if any
      *logfile,              // Log file, if any
      *system_name;          // System name, if any
  pappl_loglevel_t loglevel; // Log level
  int port = 0;              // Port number, if any
  pappl_soptions_t soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_SECURITY;
  // System options
  static pappl_version_t versions[1] = // Software versions
      {
          {"brf", "", 1.0, 1.0}};

  // Parse standard log and server options...
  if ((val = cupsGetOption("log-level", num_options, options)) != NULL)
  {
    if (!strcmp(val, "fatal"))
      loglevel = PAPPL_LOGLEVEL_FATAL;
    else if (!strcmp(val, "error"))
      loglevel = PAPPL_LOGLEVEL_ERROR;
    else if (!strcmp(val, "warn"))
      loglevel = PAPPL_LOGLEVEL_WARN;
    else if (!strcmp(val, "info"))
      loglevel = PAPPL_LOGLEVEL_INFO;
    else if (!strcmp(val, "debug"))
      loglevel = PAPPL_LOGLEVEL_DEBUG;
    else
    {
      fprintf(stderr, "brf: Bad log-level value '%s'.\n", val);
      return (NULL);
    }
  }
  else
    loglevel = PAPPL_LOGLEVEL_UNSPEC;

  logfile = cupsGetOption("log-file", num_options, options);
  hostname = cupsGetOption("server-hostname", num_options, options);
  system_name = cupsGetOption("system-name", num_options, options);

  if ((val = cupsGetOption("server-port", num_options, options)) != NULL)
  {
    if (!isdigit(*val & 255))
    {
      fprintf(stderr, "brf: Bad server-port value '%s'.\n", val);
      return (NULL);
    }
    else
      port = atoi(val);
  }

  // State file...
  if ((val = getenv("SNAP_DATA")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/brf.conf", val);
  }
  else if ((val = getenv("XDG_DATA_HOME")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/.brf.conf", val);
  }
#ifdef _WIN32
  else if ((val = getenv("USERPROFILE")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/AppData/Local/brf.conf", val);
  }
  else
  {
    papplCopyString(brf_statefile, "/brf.ini", sizeof(brf_statefile));
  }
#else
  else if ((val = getenv("HOME")) != NULL)
  {
    snprintf(brf_statefile, sizeof(brf_statefile), "%s/.brf.conf", val);
  }
  else
  {
    papplCopyString(brf_statefile, "/etc/brf.conf", sizeof(brf_statefile));
  }
#endif // _WIN32

  // Create the system object...
  if ((system = papplSystemCreate(soptions, system_name ? system_name : "Braille printer app", port, "_print,_universal", cupsGetOption("spool-directory", num_options, options), logfile ? logfile : "-", loglevel, cupsGetOption("auth-service", num_options, options), /* tls_only */ false)) == NULL)
    return (NULL);

  papplSystemAddListeners(system, NULL);
  papplSystemSetHostName(system, hostname);

  papplSystemSetMIMECallback(system, mime_cb, NULL);

  papplSystemAddMIMEFilter(system, "application/pdf", brf_TESTPAGE_MIMETYPE, BRFTestFilterCB, NULL);

  papplSystemSetPrinterDrivers(system, (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])), brf_drivers, autoadd_cb, /*create_cb*/ NULL, driver_cb, system);

  papplSystemSetFooterHTML(system, "Copyright &copy; 2024 by Arun Patwa. All rights reserved.");

  papplSystemSetSaveCallback(system, (pappl_save_cb_t)papplSystemSaveState, (void *)brf_statefile);

  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);

  fprintf(stderr, "brf: statefile='%s'\n", brf_statefile);

  // if (!papplSystemLoadState(system, brf_statefile))
  // {
    // No old state, use defaults and auto-add printers...

    papplSystemSetDNSSDName(system, system_name ? system_name : "brf");

    papplLog(system, PAPPL_LOGLEVEL_INFO, "Auto-adding printers...");
    papplDeviceList(PAPPL_DEVTYPE_USB, (pappl_device_cb_t)printer_cb, system, papplLogDevice, system);
    // mkdir("/home/arun/BRF");
    if(
    !papplPrinterCreate(system, 0, "cups-brf", "gen_brf", NULL, "file:///home/arun/BRF-Embosser")){
      printf("**************%d\n", errno);
    }
    else{
      printf("*******************Added******************\n");
    }
  // }

  return (system);
}

// 'BRFTestFilterCB()' - Print a test page.

// Items to configure the properties of this Printer Application
// These items do not change while the Printer Application is running

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

} pr_printer_app_config_t;

typedef struct brf_printer_app_global_data_s
{
  pr_printer_app_config_t *config;
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

typedef struct brf_spooling_conversion_s
{
  char *srctype;                         // Input data type
  char *dsttype;                         // Output data type
  int num_filters;                       // Number of filters
  cf_filter_filter_in_chain_t filters[]; // List of filters with
                                         // parameters
} brf_spooling_conversion_t;


char *filter_envp[] = {  "PPD=/home/arun/open-printing/Braille-printer-app/BRF.ppd","CONTENT_TYPE=application/pdf", NULL };

static cf_filter_external_t texttobrf_filter = {

  .filter = "/usr/lib/cups/filter/texttobrf",
  .envp = filter_envp,

};



static brf_spooling_conversion_t brf_convert_pdf_to_brf =
    {
        "application/pdf",
        "application/vnd.cups-brf",
        1,
        {{cfFilterExternal,
          &texttobrf_filter,
          "texttobrf"}}};


bool // O - `true` on success, `false` on failure
BRFTestFilterCB(
    pappl_job_t *job,       // I - Job
    pappl_device_t *device, // I - Output device
    void *cbdata)           // I - Callback data (not used)
{
    int i;
    brf_printer_app_global_data_t *global_data=(brf_printer_app_global_data_t *)cbdata;;
    brf_cups_device_data_t *device_data = NULL;
    // brf_job_data_t * job_data; 
    const char *informat;
    const char *filename; // Input filename
    int fd;               // Input file descriptor
    brf_spooling_conversion_t *conversion;     // Spooling conversion to use for pre-filtering
    cups_array_t *spooling_conversions;
    cf_filter_filter_in_chain_t *chain_filter, // Filter from PPD file
                                *print;
    cf_filter_external_t *filter_data_ext;
    brf_print_filter_function_data_t *print_params;
    cf_filter_data_t *filter_data;
    cups_array_t *chain;
    int nullfd;           // File descriptor for /dev/null

    bool ret = false;    // Return value
    int num_options = 0; // Number of PPD print options

    pappl_pr_options_t *job_options = papplJobCreatePrintOptions(job, INT_MAX, 0);

    cups_option_t *options = NULL;
    cf_filter_external_t *ext_filter_params;

    pappl_pr_driver_data_t driver_data;
    pappl_printer_t *printer = papplJobGetPrinter(job);
    const char *device_uri = papplPrinterGetDeviceURI(printer);


    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Entering BRFTestFilterCB()");

    // Prepare job data to be supplied to filter functions/CUPS filters called during job execution
    filter_data = (cf_filter_data_t *)calloc(1, sizeof(cf_filter_data_t));
    if (!filter_data) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for filter_data");
        return false;
    }
    
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Allocated memory for filter_data");

    // Initialize filter_data fields
    filter_data->printer = strdup(papplPrinterGetName(printer));
    if (!filter_data->printer) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for printer name");
        return false;
    }
    
    filter_data->job_id = papplJobGetID(job);
    filter_data->job_user = strdup(papplJobGetUsername(job));
    if (!filter_data->job_user) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for job user");
        return false;
    }
    
    filter_data->job_title = strdup(papplJobGetName(job));
    if (!filter_data->job_title) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for job title");
        return false;
    }

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Job ID: %d, Job User: %s, Job Title: %s", 
                filter_data->job_id, filter_data->job_user, filter_data->job_title);


    filter_data->copies = job_options->copies;
    filter_data->num_options = num_options;
    filter_data->options = options; // PPD/filter options
    filter_data->extension = NULL;
    filter_data->back_pipe[0] = -1;
    filter_data->back_pipe[1] = -1;
    filter_data->side_pipe[0] = -1;
    filter_data->side_pipe[1] = -1;
    filter_data->logdata = job;
    filter_data->logfunc = cfCUPSLogFunc;

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Filter data initialized");

    // Open the input file...
    filename = papplJobGetFilename(job);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Opening input file: %s", filename);
    if ((fd = open(filename, O_RDONLY)) < 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open input file '%s': %s", filename, strerror(errno));
        return false;
    }

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Input file opened successfully");

    // Get input file format
    informat = papplJobGetFormat(job);
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Input file format: %s", informat);

    // Initialize filter_data_ext for external filter
    filter_data_ext = (cf_filter_external_t *)calloc(1, sizeof(cf_filter_external_t));
    if (!filter_data_ext) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for filter_data_ext");
        close(fd);
        return false;
    }
    
    filter_data_ext->filter = "/usr/lib/cups/filter/texttobrf";
    filter_data_ext->num_options = 0;
    filter_data_ext->options = NULL;
    filter_data_ext->envp = NULL;


    
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Assigned external filter: %s", filter_data_ext->filter);

    // Assign spooling conversion
    conversion = &brf_convert_pdf_to_brf;

      // for (conversion =
      //      (brf_spooling_conversion_t *)
      //          cupsArrayFirst(global_data->config->spooling_conversions);
      //  conversion;
      //  conversion =
      //      (brf_spooling_conversion_t *)
      //          cupsArrayNext(global_data->config->spooling_conversions))
   
  // {
  //   if (strcmp(conversion->srctype, informat) == 0)
  //     printf("***************%d**********",informat);
  //     // printf("*********************inside if****%d***********\n",conversion);
  //     break;
  // }

  //  printf("*************************%d***********\n",conversion->srctype);

    if (conversion == NULL) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "No pre-filter found for input format %s", informat);
        close(fd);
        return false;
    }
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Using spooling conversion from %s to %s", conversion->srctype, conversion->dsttype);

    // Set input and output formats for the filter chain
    filter_data->content_type = conversion->srctype;
    filter_data->final_content_type = conversion->dsttype;

    // Connect the job's filter_data to the backend
    if (strncmp(device_uri, "cups:", 5) == 0) {
        // Get the device data
        device_data = (brf_cups_device_data_t *)papplDeviceGetData(device);
        if (device_data == NULL) {
            papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to get device data");
            close(fd);
            return false;
        }

        // Connect the filter_data
        device_data->filter_data = filter_data;
        papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Connected filter_data to backend");
    }

    // Set up filter function chain
    chain = cupsArrayNew(NULL, NULL);

    for (int i = 0; i < conversion->num_filters; i++) {
        cupsArrayAdd(chain, &(conversion->filters[i]));
    }

    // Add print filter function at the end of the chain
    print = (cf_filter_filter_in_chain_t *)calloc(1, sizeof(cf_filter_filter_in_chain_t));
    if (!print) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for print filter");
        close(fd);
        return false;
    }

    print_params = (brf_print_filter_function_data_t *)calloc(1, sizeof(brf_print_filter_function_data_t));
    if (!print_params) {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for print_params");
        close(fd);
        return false;
    }

    print_params->device = device;
    print_params->device_uri = device_uri;
    print_params->job = job;
    print_params->global_data = global_data;
    print->function = brf_print_filter_function;
    print->parameters = print_params;
    print->name = "Backend";
    
    cupsArrayAdd(chain, print);
    
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Filter chain set up");

    // Fire up the filter functions
    papplJobSetImpressions(job, 1);
    nullfd = open("/dev/null", O_RDWR);

    if (cfFilterChain(fd, nullfd, 1, filter_data, chain) == 0) {
        papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "cfFilterChain() completed successfully");
        ret = true;
    } else {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "cfFilterChain() failed");
    }

   papplJobDeletePrintOptions(job_options);


    
    close(fd);
    close(nullfd);
    return ret;
}

int brf_print_filter_function(int inputfd, int outputfd, int inputseekable, cf_filter_data_t *data, void *parameters) {
    ssize_t bytes;
    char buffer[65536];
    cf_logfunc_t log = data->logfunc;
    void *ld = data->logdata;
    brf_print_filter_function_data_t *params = (brf_print_filter_function_data_t *)parameters;
    pappl_device_t *device = params->device;
    pappl_job_t *job = params->job;
    pappl_printer_t *printer;
    brf_printer_app_global_data_t *global_data = params->global_data;
    char filename[2048];
    int debug_fd = -1;

    // if (papplSystemGetLogLevel(global_data->system) == PAPPL_LOGLEVEL_DEBUG) {
    //     printer = papplJobGetPrinter(job);
    //     snprintf(filename, sizeof(filename), "/tmp/brf-printer-app.%d.log", papplPrinterGetID(printer));
    //     debug_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    // }

    while ((bytes = read(inputfd, buffer, sizeof(buffer))) > 0) {
          // fprintf(stderr,"*****************FIRST**%zd**********\n",bytes);
        if (debug_fd >= 0)
            write(debug_fd, buffer, bytes);
        if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0){
          // fprintf(stderr,"************SECOND*******%zd**********\n",bytes);
        return 1;
        }
    }

    papplDeviceFlush(device);
    
    if (debug_fd >= 0)
        close(debug_fd);
    return 0;
}



// bool // O - `true` on success, `false` on failure
// BRFTestFilterCB(
//     pappl_job_t *job,       // I - Job
//     pappl_device_t *device, // I - Output device
//     void *cbdata)           // I - Callback data (not used)
// {
//     const char *filename;      // Input filename
//     const char *output_file;   // Output filename for BRF
//     const char *logFile = "/tmp/brf_conversion_log.txt"; // Log file for conversion errors (optional)
//     bool ret = false;          // Return value
//     int output_fd = -1;        // File descriptor for output BRF file
//     ssize_t bytes;             // Number of bytes read/written
//     char buffer[65536];        // Buffer to hold data during file transfer

//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Entering BRFTestFilterCB()");

//     // Get the input filename from the job
//     filename = papplJobGetFilename(job);
//     if (!filename) {
//         papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to get input filename from job.");
//         return false;
//     }
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Input file: %s", filename);

//     // Create a temporary output file for the BRF result
//     output_file = "/tmp/output.brf";  // You can dynamically generate this or use a job-specific path
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Output BRF file will be saved to: %s", output_file);

//     // Call the BRF conversion function
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Starting BRF conversion using lbu_translateTextFile");

  


//     if (lbu_translateTextFile(NULL, filename, output_file, logFile, NULL, 0) != 0)
//     {
//         papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to convert text to BRF using lbu_translateTextFile");
//         // Log the contents of the logFile if conversion fails
//         FILE *log_fd = fopen(logFile, "r");
//         if (log_fd) {
//             papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Error details from log file:");
//             char log_buffer[1024];
//             while (fgets(log_buffer, sizeof(log_buffer), log_fd)) {
//                 papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "%s", log_buffer);
//             }
//             fclose(log_fd);
//         } else {
//             papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Could not open conversion log file: %s", logFile);
//         }
//         return false;
//     }
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Text to BRF conversion completed successfully");

//     // Attempt to open the output BRF file
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Opening BRF output file: %s", output_file);
//     if ((output_fd = open(output_file, O_RDONLY)) < 0) {
//         papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to open BRF output file '%s': %s", output_file, strerror(errno));
//         return false;
//     }
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "BRF output file opened successfully");

//     // Send BRF output to the device
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Sending BRF output to the device...");
//     while ((bytes = read(output_fd, buffer, sizeof(buffer))) > 0) {
//         papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Read %zd bytes from BRF output file", bytes);
//         if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0) {
//             papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to write to device.");
//             close(output_fd);
//             return false;
//         }
//         papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Wrote %zd bytes to the device", bytes);
//     }

//     if (bytes < 0) {
//         papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Error reading BRF output file: %s", strerror(errno));
//         close(output_fd);
//         return false;
//     }

//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "BRF output sent to device successfully");

//     // Close the output file
//     if (output_fd >= 0) {
//         papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Closing BRF output file.");
//         close(output_fd);
//     }

//     // Set the return value to true if everything succeeded
//     ret = true;
    
//     papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Exiting BRFTestFilterCB()");
//     return ret;
// }



// bool // O - `true` on success, `false` on failure
// BRFTestFilterCB(
//     pappl_job_t *job,       // I - Job
//     pappl_device_t *device, // I - Output device
//     void *cbdata)           // I - Callback data (not used)
// {
//   pappl_pr_options_t *job_options = papplJobCreatePrintOptions(job, INT_MAX, 0);
//   brf_cups_device_data_t *device_data = NULL;
//   brf_print_filter_function_data_t *print_params;
//   const char *filename;    // Input filename
//   char output_file[1024];  // Output filename
//   char logFile[1024];      // Log file for the conversion
//   int fd;                  // Input file descriptor
//   bool ret = false;        // Return value
//   FILE *output_fp = NULL;  // File pointer for output file
//   char buffer[8192];       // Buffer for reading the BRF file
//   ssize_t bytes_read;      // Number of bytes read from the file

//   pappl_printer_t *printer = papplJobGetPrinter(job);
//   const char *device_uri = papplPrinterGetDeviceURI(printer);

//   // Open the input file...
//   filename = papplJobGetFilename(job);
//   if ((fd = open(filename, O_RDONLY)) < 0)
//   {
//     papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open input file '%s' for printing: %s",
//                 filename, strerror(errno));
//     return (false);
//   }

//   // Prepare output and log file paths
//   snprintf(output_file, sizeof(output_file), "/tmp/%d_output.brf", papplJobGetID(job));
//   snprintf(logFile, sizeof(logFile), "/tmp/%d_log.txt", papplJobGetID(job));

//   papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Starting text to BRF conversion using lbu_translateFile");

//   // Call the conversion function
//   if (lbu_translateTextFile(NULL, filename, output_file, logFile, NULL, 0) != 0)
//   {
//     papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to convert text to BRF using lbu_translateFile");
//     return (false);
//   }
//   else{
//     printf("*******************lbutranslate failed***%d*****\n",errno);
//   }

//   papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Successfully converted text to BRF: %s", output_file);

//   // Open the output file for reading
//   output_fp = fopen(output_file, "rb");
//   if (!output_fp)
//   {
//     papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open BRF output file '%s' for sending to the device: %s",
//                 output_file, strerror(errno));
//     return (false);
//   }

//   // Connect the job's output to the device (send output to the printer)
//   papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Sending BRF file to device: %s", device_uri);

//   while ((bytes_read = fread(buffer, 1, sizeof(buffer), output_fp)) > 0)
//   {
//     if (papplDeviceWrite(device, buffer, (size_t)bytes_read) < 0)
//     {
//       papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to send data to device: %s", strerror(errno));
//       fclose(output_fp);
//       return (false);
//     }
//   }

//   fclose(output_fp);
//   papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "BRF file successfully sent to the device");

//   // Set impressions (this may depend on your application)
//   papplJobSetImpressions(job, 1);

//   // After conversion and processing
//   ret = true;

//   return ret;
// }