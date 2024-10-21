#include <strings.h>
#include <cupsfilters/log.h>
#include <cupsfilters/filter.h>
#include <ppd/ppd-filter.h>
#include <limits.h>
#include <pappl/pappl.h>
// #include <liblouisutdml/liblouisutdml.h>
#include <cups/backend.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include "brf-printer.h"


#define SYSTEM_NAME "BRF Printer Application"
#define SYSTEM_PACKAGE_NAME "brf-printer-app"
#define SYSTEM_VERSION_STR "1.0"
#define SYSTEM_VERSION_ARR_0 1
#define SYSTEM_VERSION_ARR_1 0
#define SYSTEM_VERSION_ARR_2 0
#define SYSTEM_VERSION_ARR_3 0
#define SYSTEM_WEB_IF_FOOTER "Copyright &copy; 2024 by Arun Patwa"


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


int
main(int  argc,				// I - Number of command-line arguments
     char *argv[])			// I - Command-line arguments
{
  cups_array_t *spooling_conversions,
               *stream_formats,
               *driver_selection_regex_list;

  // Array of spooling conversions, most desirables first
  //
  // Here we prefer not converting into another format
  // Keeping vector formats (like PS -> PDF) is usually more desirable
  // but as many printers have buggy PS interpreters we prefer converting
  // PDF to Raster and not to PS
  driver_selection_regex_list = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(spooling_conversions, (void *)&brf_convert_pdf_to_brf);
  cupsArrayAdd(spooling_conversions, (void *)&brf_convert_text_to_brf);
//   cupsArrayAdd(spooling_conversions, (void *)&brf_convert_html_to_brf);
//   cupsArrayAdd(spooling_conversions, (void *)&brf_convert_doc_to_brf);
//   cupsArrayAdd(spooling_conversions, (void *)&brf_convert_jpeg_to_brf);
//   cupsArrayAdd(spooling_conversions, (void *)&brf_convert_vector_to_brf);

  // Array of stream formats, most desirables first
  //
  // PDF comes last because it is generally not streamable.
  // PostScript comes second as it is Ghostscript's streamable
//   // input format.
//   stream_formats = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
//   cupsArrayAdd(stream_formats, (void *)&PR_STREAM_CUPS_RASTER);
//   cupsArrayAdd(stream_formats, (void *)&PR_STREAM_POSTSCRIPT);
//   cupsArrayAdd(stream_formats, (void *)&PR_STREAM_PDF);

  // Array of regular expressions for driver prioritization
  driver_selection_regex_list = cupsArrayNew(NULL, NULL, NULL, 0, NULL, NULL);
  cupsArrayAdd(driver_selection_regex_list, "-brf-");
//   cupsArrayAdd(driver_selection_regex_list, "-postscript-");
//   cupsArrayAdd(driver_selection_regex_list, "-hl-1250-");
//   cupsArrayAdd(driver_selection_regex_list, "-hl-7-x-0-");
//   cupsArrayAdd(driver_selection_regex_list, "-pxlcolor-");
//   cupsArrayAdd(driver_selection_regex_list, "-pxlmono-");
//   cupsArrayAdd(driver_selection_regex_list, "-ljet-4-d-");
//   cupsArrayAdd(driver_selection_regex_list, "-ljet-4-");
//   cupsArrayAdd(driver_selection_regex_list, "-gutenprint-");

  // Configuration record of the Printer Application
  brf_printer_app_config_t printer_app_config =
  {
    SYSTEM_NAME,              // Display name for Printer Application
    SYSTEM_PACKAGE_NAME,      // Package/executable name
    SYSTEM_VERSION_STR,       // Version as a string
    {
      SYSTEM_VERSION_ARR_0,   // Version 1st number
      SYSTEM_VERSION_ARR_1,   //         2nd
      SYSTEM_VERSION_ARR_2,   //         3rd
      SYSTEM_VERSION_ARR_3    //         4th
    },
    autoadd_cb,           // Auto-add (driver assignment) callback
    prIdentify,              // Printer identify callback
    prTestPage,              // Test page print callback
    prSetupAddPPDFilesPage, // Set up "Add PPD Files" web interface page
    prSetupDeviceSettingsPage, // Set up "Device Settings" printer web
                              // interface page
    spooling_conversions,     // Array of data format conversion rules for
                              // printing in spooling mode
    stream_formats,           // Arrray for stream formats to be generated
                              // when printing in streaming mode
    "driverless, driverless-fax, ipp, ipps, http, https",
                              // CUPS backends to be ignored
    "", //"hp, gutenprint53+usb",
                              // CUPS backends to be used exclusively
                              // If empty all but the ignored backends are used
    TESTPAGE,                 // Test page (printable file), used by the
                              // standard test print callback prTestPage()
    " +Foomatic/(.+)$| +- +CUPS\\+(Gutenprint)",
                              // Regular expression to separate the
                              // extra information after make/model in
                              // the PPD's *NickName. Also extracts a
                              // contained driver name (by using
                              // parentheses)
    driver_selection_regex_list
                              // Regular expression for the driver
                              // auto-selection to prioritize a driver
                              // when there is more than one for a
                              // given printer. If a regular
                              // expression matches on the driver
                              // name, the driver gets priority. If
                              // there is more than one matching
                              // driver, the driver name on which the
                              // earlier regular expression in the
                              // list matches, gets the priority.
  };

  // If the "driverless" utility is under the CUPS backends or under
  // the PPD-generating executables, tell it to not browse the network
  // for supported (driverless) printers but exit immediately, as this
  // Printer Application is for using printers with installed CUPS
  // drivers.
  putenv("NO_DRIVERLESS_PPDS=1");

  return (prRetroFitPrinterApp(&printer_app_config, argc, argv));
}
