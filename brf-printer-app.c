// Include necessary headers...
#include <strings.h>
#include <cupsfilters/log.h>
#include <cupsfilters/filter.h>
#include <ppd/ppd-filter.h>
#include <limits.h>
#include <pappl/pappl.h>

#define brf_TESTPAGE_HEADER    "T*E*S*T*P*A*G*E*"
#define brf_TESTPAGE_MIMETYPE  "application/vnd.cups-paged-brf"

extern bool brf_gen(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata);
extern char* strdup(const char*);

// Local functions...
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
  { "gen_brf",  "Generic", NULL, NULL },
};

static char brf_statefile[1024]; // State file

// 'main()' - Main entry for brf.
int main(int argc, char *argv[])
{
  return (papplMainloop(argc, argv,
                        "1.0",
                        NULL,
                        (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])),
                        brf_drivers, autoadd_cb, driver_cb,
                        /*subcmd_name*/NULL, /*subcmd_cb*/NULL,
                        system_cb,
                        /*usage_cb*/NULL,
                        /*data*/NULL));
}

// 'autoadd_cb()' - Determine the proper driver for a given printer.
static const char *autoadd_cb(const char *device_info, const char *device_uri, const char *device_id, void *cbdata)
{
  int num_did;
  cups_option_t *did;
  const char *make, *best_name = NULL;
  int best_score = 0;

  (void)device_info;

  num_did = papplDeviceParseID(device_id, &did);

  if ((make = cupsGetOption("MANUFACTURER", num_did, did)) == NULL)
    if ((make = cupsGetOption("MANU", num_did, did)) == NULL)
      make = cupsGetOption("MFG", num_did, did);

  cupsFreeOptions(num_did, did);

  return (best_name);
}

// 'match_id()' - Compare two IEEE-1284 device IDs and return a score.
static int match_id(int num_did, cups_option_t *did, const char *match_id)
{
  int i, score = 0, num_mid;
  cups_option_t *mid, *current;
  const char *value, *valptr;

  if ((num_mid = papplDeviceParseID(match_id, &mid)) == 0)
    return (0);

  for (i = num_mid, current = mid; i > 0; i--, current++)
  {
    if ((value = cupsGetOption(current->name, num_did, did)) == NULL)
    {
      score = 0;
      break;
    }

    if (!strcasecmp(current->value, value))
    {
      score += 2;
    }
    else if ((valptr = strstr(value, current->value)) != NULL)
    {
      size_t mlen = strlen(current->value);
      if ((valptr == value || valptr[-1] == ',') && (!valptr[mlen] || valptr[mlen] == ','))
      {
        score++;
      }
      else
      {
        score = 0;
        break;
      }
    }
    else
    {
      score = 0;
      break;
    }
  }

  return score;
}

// 'driver_cb()' - Main driver callback.
static bool driver_cb(pappl_system_t *system, const char *driver_name, const char *device_uri, const char *device_id, pappl_pr_driver_data_t *data, ipp_t **attrs, void *cbdata)
{
  int i;

  for (i = 0; i < (int)(sizeof(brf_drivers) / sizeof(brf_drivers[0])); i++)
  {
    if (!strcmp(driver_name, brf_drivers[i].name))
    {
      papplCopyString(data->make_and_model, brf_drivers[i].description, sizeof(data->make_and_model));
      break;
    }
  }

  data->ppm = 1;
  data->color_supported = PAPPL_COLOR_MODE_AUTO | PAPPL_COLOR_MODE_MONOCHROME;
  data->color_default = PAPPL_COLOR_MODE_MONOCHROME;
  data->raster_types = PAPPL_PWG_RASTER_TYPE_BLACK_8;
  data->quality_default = IPP_QUALITY_NORMAL;
  data->orient_default = IPP_ORIENT_NONE;
  data->sides_supported = PAPPL_SIDES_ONE_SIDED;
  data->sides_default = PAPPL_SIDES_ONE_SIDED;
  data->orient_default = IPP_ORIENT_NONE;

  if (!strncmp(driver_name, "gen_", 4))
    return (brf_gen(system, driver_name, device_uri, device_id, data, attrs, cbdata));
  else
    return (false);
}

// 'mime_cb()' - MIME typing callback.
static const char *mime_cb(const unsigned char *header, size_t headersize, void *data)
{
  return (brf_TESTPAGE_MIMETYPE);
}

// 'printer_cb()' - Try auto-adding printers.
static bool printer_cb(const char *device_info, const char *device_uri, const char *device_id, pappl_system_t *system)
{
  const char *driver_name = autoadd_cb(device_info, device_uri, device_id, system);

  if (driver_name)
  {
    char name[128], *nameptr;
    papplCopyString(name, device_info, sizeof(name));

    if ((nameptr = strstr(name, " (")) != NULL)
      *nameptr = '\0';

    if (!papplPrinterCreate(system, 0, name, driver_name, device_id, device_uri))
    {
      int i;
      char newname[128], number[4];
      size_t namelen = strlen(name), numberlen;

      for (i = 2; i < 100; i++)
      {
        snprintf(number, sizeof(number), " %d", i);
        numberlen = strlen(number);

        papplCopyString(newname, name, sizeof(newname));
        if ((namelen + numberlen) < sizeof(newname))
          memcpy(newname + namelen, number, numberlen + 1);
        else
          memcpy(newname + sizeof(newname) - numberlen - 1, number, numberlen + 1);

        if (papplPrinterCreate(system, 0, newname, driver_name, device_id, device_uri))
          break;
      }
    }
  }

  return (false);
}

// 'system_cb()' - Setup the system object.
static pappl_system_t *system_cb(int num_options, cups_option_t *options, void *data)
{
  pappl_system_t *system;
  const char *val, *hostname, *logfile, *system_name;
  pappl_loglevel_t loglevel;
  int port = 0;
  pappl_soptions_t soptions = PAPPL_SOPTIONS_MULTI_QUEUE | PAPPL_SOPTIONS_WEB_INTERFACE | PAPPL_SOPTIONS_WEB_LOG | PAPPL_SOPTIONS_WEB_SECURITY;
  static pappl_version_t versions[1] = { {"brf", "", 1.0, 1.0} };

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
  system_name = cupsGetOption("system-name", num_options, options);
  hostname = cupsGetOption("hostname", num_options, options);

  if ((val = cupsGetOption("port", num_options, options)) != NULL)
  {
    char *end;

    port = (int)strtol(val, &end, 10);

    if (errno == ERANGE || *end)
    {
      fprintf(stderr, "brf: Bad port value '%s'.\n", val);
      return (NULL);
    }
  }

  if (!port)
    port = 8023;

  if ((system = papplSystemCreate(soptions, system_name ? system_name : "Generic BRF System", port)) == NULL)
    return (NULL);

  if (hostname)
    papplSystemSetHostName(system, hostname);

  if (loglevel)
    papplSystemSetLogLevel(system, loglevel);

  if (logfile)
    papplSystemSetLogFile(system, logfile);

  papplSystemAddMIMEFilter(system, brf_TESTPAGE_MIMETYPE, NULL, mime_cb, BRFTestFilterCB, NULL);
  papplSystemSetVersions(system, (int)(sizeof(versions) / sizeof(versions[0])), versions);
  snprintf(brf_statefile, sizeof(brf_statefile), "%s/.brf.state", papplGetUserHomeDirectory());
  papplSystemLoadState(system, brf_statefile);
  papplSystemSetDriverSelectionCallback(system, printer_cb);

  return (system);
}

// 'BRFTestFilterCB()' - Process test BRF data.
static bool BRFTestFilterCB(pappl_job_t *job, pappl_device_t *device, void *cbdata)
{
  int inputfd = papplJobGetFD(job, NULL);
  int outputfd = papplDeviceGetFD(device);

  cf_filter_data_t filter_data;
  memset(&filter_data, 0, sizeof(cf_filter_data_t));

  filter_data.job = job;
  filter_data.device = device;
  filter_data.device_uri = papplJobGetDeviceURI(job);
  filter_data.device_id = papplJobGetDeviceID(job);

  return (brf_print_filter_function(inputfd, outputfd, 0, &filter_data, cbdata) == 0);
}

// 'brf_print_filter_function()' - Print filter function.
static int brf_print_filter_function(int inputfd, int outputfd, int inputseekable, cf_filter_data_t *data, void *parameters)
{
  char buffer[8192];
  ssize_t bytes;

  while ((bytes = read(inputfd, buffer, sizeof(buffer))) > 0)
  {
    if (write(outputfd, buffer, (size_t)bytes) < 0)
    {
      perror("brf_print_filter_function: write");
      return (-1);
    }
  }

  if (bytes < 0)
  {
    perror("brf_print_filter_function: read");
    return (-1);
  }

  return (0);
}
