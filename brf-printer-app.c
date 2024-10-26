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
#include "brf-printer.h"

// Include necessary headers...

#define brf_TESTPAGE_HEADER "T*E*S*T*P*A*G*E*"
#define brf_TESTPAGE_MIMETYPE "application/vnd.cups-brf"
#define brf_INPUT_TESTPAGE_MIMETYPE "text/plain"

extern bool brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern char *strdup(const char *);

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

  cups_array_t *spooling_conversions;
  spooling_conversions = cupsArrayNew(NULL, NULL);

  for (int i = 0; converts[i].srctype!=NULL; i++) {
      cupsArrayAdd(spooling_conversions, &converts[i]);
  }
 
  brf_printer_app_config_t printer_app_config = {
      .spooling_conversions = spooling_conversions};

  brf_printer_app_global_data_t global_data;

  memset(&global_data, 0, sizeof(global_data));

  global_data.config = &printer_app_config;

  return (papplMainloop(argc, argv,
                        "1.0",
                        NULL,
                        (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])),
                        brf_drivers, autoadd_cb, driver_cb,
                        /*subcmd_name*/ NULL, /*subcmd_cb*/ NULL,
                        system_cb,
                        /*usage_cb*/ NULL,
                        /*data*/ &global_data));
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

  else
  {
    printf("****************brfgen-not called***************\n");
    return (false);
  }
}

void BRFSetup(pappl_system_t *system, brf_printer_app_global_data_t *global_data)
{
  brf_spooling_conversion_t *conversion;

  // Track if the MIME filter was added successfully
  bool filter_added = false;

  // for (conversion = (brf_spooling_conversion_t *)cupsArrayFirst(global_data->config->spooling_conversions);
  //      conversion;
  //      conversion = (brf_spooling_conversion_t *)cupsArrayNext(global_data->config->spooling_conversions))
  // {
 for (int i = 0; converts[i].srctype!=NULL; i++) {
      conversion=&converts[i];

    // Log the attempt to add the filter
    // printf("Attempting to add MIME filter: %s -> %s\n",
    //        conversion->srctype, conversion->dsttype);

    // Call the function (void, no return value)
    papplSystemAddMIMEFilter(system, conversion->srctype, conversion->dsttype, BRFTestFilterCB, global_data);

    // Set filter_added to true after calling the function
    filter_added = true;

    // Log success after the function call
    // printf("MIME filter added for: %s -> %s\n",
    //        conversion->srctype, conversion->dsttype);
  }

  printf("****************BRFSETUP IS CALLED**********************\n");
}
// 'mime_cb()' - MIME typing callback...

static const char *                  // O - MIME media type or `NULL` if none
mime_cb(const unsigned char *header, // I - Header data
        size_t headersize,           // I - Size of header data
        void *cbdata)                // I - Callback data (not used)
{

  printf("************************mimecbcalled********************\n");
  return ("text/plain");
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

  brf_printer_app_global_data_t *global_data = (brf_printer_app_global_data_t *)data;
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
  // initialize_spooling_conversions();

  papplSystemSetMIMECallback(system, mime_cb, NULL);

  BRFSetup(system, global_data);

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

  if (
      !papplPrinterCreate(system, 0, "cups-brf", "gen_brf", NULL, "file:///home/arun/open-printing/Braille-printer-app/BRF-Embosser"))
  {
    printf("**************%d\n", errno);
  }
  else
  {
    printf("*******************Added******************\n");
  }

  return (system);
}

// 'BRFTestFilterCB()' - Print a test page.

// Items to configure the properties of this Printer Application
// These items do not change while the Printer Application is running

bool // O - `true` on success, `false` on failure
BRFTestFilterCB(
    pappl_job_t *job,       // I - Job
    pappl_device_t *device, // I - Output device
    void *cbdata)           // I - Callback data (not used)
{
  int i;
  brf_printer_app_global_data_t *global_data = (brf_printer_app_global_data_t *)cbdata;

  brf_cups_device_data_t *device_data = NULL;
  // brf_job_data_t * job_data;
  const char *informat;
  const char *filename;                  // Input filename
  int fd;                                // Input file descriptor
  brf_spooling_conversion_t *conversion; // Spooling conversion to use for pre-filtering
  cups_array_t *spooling_conversions;
  cf_filter_filter_in_chain_t *chain_filter, // Filter from PPD file
      *print;
  cf_filter_external_t *filter_data_ext;
  brf_print_filter_function_data_t *print_params;
  cf_filter_data_t *filter_data;
  cups_array_t *chain;
  int nullfd; // File descriptor for /dev/null

  bool ret = false;    // Return value
  int num_options = 0; // Number of PPD print options

  pappl_pr_options_t *job_options = papplJobCreatePrintOptions(job, INT_MAX, 0);

  cups_option_t *options = NULL;
  cf_filter_external_t *ext_filter_params;

  pappl_pr_driver_data_t driver_data;
  pappl_printer_t *printer = papplJobGetPrinter(job);
  const char *device_uri = papplPrinterGetDeviceURI(printer);

  // job_options->print_content_optimize=brf_GetFileContentType(job);

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Entering BRFTestFilterCB()");

  // Prepare job data to be supplied to filter functions/CUPS filters called during job execution
  filter_data = (cf_filter_data_t *)calloc(1, sizeof(cf_filter_data_t));
  if (!filter_data)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for filter_data");
    return false;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Allocated memory for filter_data");

  // Initialize filter_data fields
  filter_data->printer = strdup(papplPrinterGetName(printer));
  if (!filter_data->printer)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for printer name");
    return false;
  }

  filter_data->job_id = papplJobGetID(job);
  filter_data->job_user = strdup(papplJobGetUsername(job));
  if (!filter_data->job_user)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for job user");
    return false;
  }

  filter_data->job_title = strdup(papplJobGetName(job));
  if (!filter_data->job_title)
  {
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

  filter_data->logfunc =brf_JobLog; // Job log function catching page counts
                                  // ("PAGE: XX YY" messages)
  filter_data->logdata = job;
  filter_data->iscanceledfunc = brf_JobIsCanceled; // Function to indicate
                                                // whether the job got
  // canceled
  filter_data->iscanceleddata = job;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Filter data initialized");

  // Initialize filter_data_ext for external filter
  filter_data_ext = (cf_filter_external_t *)calloc(1, sizeof(cf_filter_external_t));

  if (!filter_data_ext)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for filter_data_ext");
    close(fd);
    return false;
  }

  filter_data_ext->filter = texttobrf_filter.filter;
  filter_data_ext->num_options = 0;
  filter_data_ext->options = NULL;
  filter_data_ext->envp = NULL;

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Assigned external filter: %s", filter_data_ext->filter);

  // Open the input file...
  filename = papplJobGetFilename(job);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Opening input file: %s", filename);
  if ((fd = open(filename, O_RDONLY)) < 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Unable to open input file '%s': %s", filename, strerror(errno));
    return false;
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Input file opened successfully");

  
  // Set up filter function chain
    chain = cupsArrayNew(NULL, NULL);

  // Get input file format
  informat = papplJobGetFormat(job);
  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Input file format: %s", informat);

  const char *currentFormat = informat;


  while (strcmp(currentFormat, "application/vnd.cups-brf") != 0)
  {
    // for (conversion = (brf_spooling_conversion_t *)cupsArrayFirst(global_data->config->spooling_conversions);
    //      conversion;
    //      conversion = (brf_spooling_conversion_t *)cupsArrayNext(global_data->config->spooling_conversions))
    // {
     for (int i = 0; converts[i].srctype!=NULL; i++) {
      conversion=&converts[i];
      // Compare source type with informat
      if (strcmp(conversion->srctype, informat) == 0)
      {
        // Log the match details
        printf("***********conversion_informat****%s**********\n", informat);
        printf("***********conversion_srctype****%s**********\n", conversion->srctype);
        break; // Exit the loop when a match is found
      }
    }
    printf("****************after if conversion_srctype*********%s***********\n", conversion->srctype);

    if (conversion == NULL)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "No pre-filter found for input format %s", informat);
      close(fd);
      return false;
    }
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Using spooling conversion from %s to %s", conversion->srctype, conversion->dsttype);

    // Set input and output formats for the filter chain
    filter_data->content_type = conversion->srctype;
    filter_data->final_content_type = conversion->dsttype;

    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Converting input file to format: %s", conversion->dsttype);

    // Connect the job's filter_data to the backend
    if (strncmp(device_uri, "cups:", 5) == 0)
    {
      // Get the device data
      device_data = (brf_cups_device_data_t *)papplDeviceGetData(device);
      if (device_data == NULL)
      {
        papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to get device data");
        close(fd);
        return false;
      }

      // Connect the filter_data
      device_data->filter_data = filter_data;
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Connected filter_data to backend");
    }

    cupsArrayAdd(chain, &(conversion->filters));

    currentFormat = conversion->dsttype;
  }

  // Add print filter function at the end of the chain
  print = (cf_filter_filter_in_chain_t *)calloc(1, sizeof(cf_filter_filter_in_chain_t));
  
  if (!print)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "Failed to allocate memory for print filter");
    close(fd);
    return false;
  }

  print_params = (brf_print_filter_function_data_t *)calloc(1, sizeof(brf_print_filter_function_data_t));
  if (!print_params)
  {
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

  if (cfFilterChain(fd, nullfd, 1, filter_data, chain) == 0)
  {
    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "cfFilterChain() completed successfully");
    ret = true;
  }
  else
  {
    papplLogJob(job, PAPPL_LOGLEVEL_ERROR, "cfFilterChain() failed");
  }

  papplJobDeletePrintOptions(job_options);

  close(fd);
  close(nullfd);
  return ret;
}

int brf_print_filter_function(int inputfd, int outputfd, int inputseekable, cf_filter_data_t *data, void *parameters)
{
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

  while ((bytes = read(inputfd, buffer, sizeof(buffer))) > 0)
  {
    // fprintf(stderr,"*****************FIRST**%zd**********\n",bytes);
    if (debug_fd >= 0)
      (void) write(debug_fd, buffer, bytes);

    if (papplDeviceWrite(device, buffer, (size_t)bytes) < 0)
    {
      // fprintf(stderr,"************SECOND*******%zd**********\n",bytes);
      return 1;
    }
  }

  papplDeviceFlush(device);

  if (debug_fd >= 0)
    close(debug_fd);
  return 0;
}

pappl_content_t
brf_GetFileContentType(pappl_job_t *job)
{
  int i, j, k;
  const char *informat,
      *filename,
      *found;
  char command[512],
      line[256];
  char *p, *q;
  int creatorline_found = 0;
  pappl_content_t content_type;

  // In the fields "Creator", "Creator Tool", and/or "Producer" of the
  // metadata of a PDF file one usually find the name of the
  // application which created the file. We use thse names to find out
  // the type of content to expect in the file.
  const char *automatic[] =
      {// PAPPL_CONTENT_AUTO
       NULL};
  const char *graphics[] =
      {          // PAPPL_CONTENT_GRAPHIC
       "Draw",   // LibreOffice
       "Charts", // LibreOffice
       "Karbon", // KDE Calligra
       "Flow",   // KDE Calligra
       "Inkscape",
       NULL};
  const char *photo[] =
      {              // PAPPL_CONTENT_PHOTO
       "imagetopdf", // CUPS
       "RawTherapee",
       "Darktable",
       "digiKam",
       "Geeqie",
       "GIMP",
       "eog",  // GNOME
       "Skia", // Google Photos on Android 11 (tested with Pixel 5)
       "ImageMagick",
       "GraphicsMagick",
       "Krita",      // KDE
       "Photoshop",  // Adobe
       "Lightroom",  // Adobe
       "Camera Raw", // Adobe
       "SilkyPix",
       "Capture One",
       "Photolab",
       "DxO",
       NULL};
  const char *text[] =
      {             // PAPPL_CONTENT_TEXT
       "texttopdf", // CUPS
       "GEdit",     // GNOME
       "Writer",    // LibreOffice
       "Word",      // Microsoft Office
       "Words",     // KDE Calligra
       "Kexi",      // KDE Calligra
       "Plan",      // KDE Calligra
       "Braindump", // KDE Calligra
       "Author",    // KDE Calligra
       "Base",      // LibreOffice
       "Math",      // LibreOffice
       "Pages",     // Mac Office
       "Thunderbird",
       "Bluefish", // IDEs
       "Geany",    // ...
       "KATE",
       "Eclipse",
       "Brackets",
       "Atom",
       "Sublime",
       "Visual Studio",
       "GNOME Builder",
       "Spacemacs",
       "Atom",
       "CodeLite", // ...
       "KDevelop", // IDEs
       "LaTeX",
       "TeX",
       NULL};
  const char *text_graphics[] =
      {          // PAPPL_CONTENT_TEXT_AND_GRAPHIC
       "evince", // GNOME
       "Okular", // KDE
       "Chrome",
       "Chromium",
       "Firefox",
       "Impress",  // LibreOffice
       "Calc",     // LibreOffice
       "Calligra", // KDE
       "QuarkXPress",
       "InDesign", // Adobe
       "WPS Presentation",
       "Keynote",    // Mac Office
       "Numbers",    // Mac Office
       "Google",     // Google Docs
       "PowerPoint", // Microsoft Office
       "Excel",      // Microsoft Office
       "Sheets",     // KDE Calligra
       "Stage",      // KDE Calligra
       NULL};

  const char **creating_apps[] =
      {
          automatic,
          graphics,
          photo,
          text,
          text_graphics,
          NULL};

  const char *const fields[] =
      {
          "Producer",
          "Creator",
          "Creator Tool",
          NULL};

  found = NULL;
  content_type = PAPPL_CONTENT_AUTO;
  informat = papplJobGetFormat(job);
  if (!strcmp(informat, "image/jpeg")) // Photos
    content_type = PAPPL_CONTENT_PHOTO;
  else if (!strcmp(informat, "image/png")) // Screen shots
    content_type = PAPPL_CONTENT_GRAPHIC;
  else if (!strcmp(informat, "application/pdf")) // PDF, creating app in
                                                 // metadata
  {
    filename = papplJobGetFilename(job);
    // Run one of the command "pdfinfo" or "exiftool" with the input file,
    // use the first which gets found
    snprintf(command, sizeof(command),
             "pdfinfo %s 2>/dev/null || exiftool %s 2>/dev/null",
             filename, filename);
    FILE *pd = popen(command, "r");
    if (!pd)
    {
      papplLogJob(job, PAPPL_LOGLEVEL_WARN,
                  "Unable to get PDF metadata from %s with both pdfinfo and exiftool",
                  filename);
    }
    else
    {
      while (fgets(line, sizeof(line), pd))
      {
        p = line;
        while (isspace(*p))
          p++;
        for (i = 0; fields[i]; i++)
          if (strncasecmp(p, fields[i], strlen(fields[i])) == 0 &&
              (isspace(*(p + strlen(fields[i]))) ||
               *(p + strlen(fields[i])) == ':'))
            break;
        if (fields[i])
        {
          p += strlen(fields[i]);
          while (isspace(*p))
            p++;
          if (*p == ':')
          {
            p++;
            while (isspace(*p))
              p++;
            while ((q = p + strlen(p) - 1) && (*q == '\n' || *q == '\r'))
              *q = '\0';
            papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
                        "PDF metadata line: %s: %s", fields[i], p);
            creatorline_found = 1;
            for (j = 0; j < 5; j++)
            {
              for (k = 0; creating_apps[j][k]; k++)
              {
                while ((q = strcasestr(p, creating_apps[j][k])))
                  if (!isalnum(*(q - 1)) &&
                      !isalnum(*(q + strlen(creating_apps[j][k]))))
                  {
                    found = creating_apps[j][k];
                    content_type = 1 << j;
                    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
                                "  Found: %s", creating_apps[j][k]);
                    break;
                  }
                  else
                    p = q;
                if (found)
                  break;
              }
              if (found)
                break;
            }
          }
        }
        if (found)
          break;
      }
      pclose(pd);
    }
    if (creatorline_found == 0)
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
                  "No suitable PDF metadata line found");
  }

  papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
              "Input file format: %s%s%s%s -> Content optimization: %s",
              informat,
              found ? " (" : "", found ? found : "", found ? ")" : "",
              (content_type == PAPPL_CONTENT_AUTO ? "No optimization" : (content_type == PAPPL_CONTENT_PHOTO ? "Photo" : (content_type == PAPPL_CONTENT_GRAPHIC ? "Graphics" : (content_type == PAPPL_CONTENT_TEXT ? "Text" : "Text and graphics")))));

  return (content_type);
}

//
// 'brf_JobIsCanceled()' - Return 1 if the job is canceled, which is
//                        the case when papplJobIsCanceled() returns
//                        true.
//

int brf_JobIsCanceled(void *data)
{
  pappl_job_t *job = (pappl_job_t *)data;

  return (papplJobIsCanceled(job) ? 1 : 0);
}

//
// 'brf_JobLog()' - Job log function which calls
//                 papplJobSetImpressionsCompleted() on page logs of
//                 filter functions
//

void brf_JobLog(void *data,
                cf_loglevel_t level,
                const char *message,
                ...)
{
  va_list arglist;
  pappl_job_t *job = (pappl_job_t *)data;
  char buf[1024];
  int page, copies;

  va_start(arglist, message);
  vsnprintf(buf, sizeof(buf) - 1, message, arglist);
  fflush(stdout);
  va_end(arglist);
  if (level == CF_LOGLEVEL_CONTROL)
  {
    if (sscanf(buf, "PAGE: %d %d", &page, &copies) == 2)
    {
      papplJobSetImpressionsCompleted(job, copies);
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Printing page %d, %d copies",
                  page, copies);
    }
    else
      papplLogJob(job, PAPPL_LOGLEVEL_DEBUG, "Unused control message: %s",
                  buf);
  }
  else
    papplLogJob(job, (pappl_loglevel_t)level, "%s", buf);
}