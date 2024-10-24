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



pappl_content_t
brf_GetFileContentType(pappl_job_t *job)
{
  int        i, j, k;
  const char *informat,
             *filename,
             *found;
  char       command[512],
             line[256];
  char       *p, *q;
  int        creatorline_found = 0;
  pappl_content_t content_type;

  // In the fields "Creator", "Creator Tool", and/or "Producer" of the
  // metadata of a PDF file one usually find the name of the
  // application which created the file. We use thse names to find out
  // the type of content to expect in the file.
  const char *automatic[] =
  { // PAPPL_CONTENT_AUTO
    NULL
  };
  const char *graphics[] =
  { // PAPPL_CONTENT_GRAPHIC
    "Draw",              // LibreOffice
    "Charts",            // LibreOffice
    "Karbon",            // KDE Calligra
    "Flow",              // KDE Calligra
    "Inkscape",
    NULL
  };
  const char *photo[] =
  { // PAPPL_CONTENT_PHOTO
    "imagetopdf",        // CUPS
    "RawTherapee",
    "Darktable",
    "digiKam",
    "Geeqie",
    "GIMP",
    "eog",               // GNOME
    "Skia",              // Google Photos on Android 11 (tested with Pixel 5)
    "ImageMagick",
    "GraphicsMagick",
    "Krita",             // KDE
    "Photoshop",         // Adobe
    "Lightroom",         // Adobe
    "Camera Raw",        // Adobe
    "SilkyPix",
    "Capture One",
    "Photolab",
    "DxO",
    NULL
  };
  const char *text[] =
  { // PAPPL_CONTENT_TEXT
    "texttopdf",         // CUPS
    "GEdit",             // GNOME
    "Writer",            // LibreOffice
    "Word",              // Microsoft Office
    "Words",             // KDE Calligra
    "Kexi",              // KDE Calligra
    "Plan",              // KDE Calligra
    "Braindump",         // KDE Calligra
    "Author",            // KDE Calligra
    "Base",              // LibreOffice
    "Math",              // LibreOffice
    "Pages",             // Mac Office
    "Thunderbird",
    "Bluefish",          // IDEs
    "Geany",             // ...
    "KATE",
    "Eclipse",
    "Brackets",
    "Atom",
    "Sublime",
    "Visual Studio",
    "GNOME Builder",
    "Spacemacs",
    "Atom",
    "CodeLite",          // ...
    "KDevelop",          // IDEs
    "LaTeX",
    "TeX",
    NULL
  };
  const char *text_graphics[] =
  { // PAPPL_CONTENT_TEXT_AND_GRAPHIC
    "evince",            // GNOME
    "Okular",            // KDE
    "Chrome",
    "Chromium",
    "Firefox",
    "Impress",           // LibreOffice
    "Calc",              // LibreOffice
    "Calligra",          // KDE
    "QuarkXPress",
    "InDesign",          // Adobe
    "WPS Presentation",
    "Keynote",           // Mac Office
    "Numbers",           // Mac Office
    "Google",            // Google Docs
    "PowerPoint",        // Microsoft Office
    "Excel",             // Microsoft Office
    "Sheets",            // KDE Calligra
    "Stage",             // KDE Calligra
    NULL
  };

  const char **creating_apps[] =
  {
    automatic,
    graphics,
    photo,
    text,
    text_graphics,
    NULL
  };

  const char * const fields[] =
  {
    "Producer",
    "Creator",
    "Creator Tool",
    NULL
  };

  
  found = NULL;
  content_type = PAPPL_CONTENT_AUTO;
  informat = papplJobGetFormat(job);
  if (!strcmp(informat, "image/jpeg"))            // Photos
    content_type = PAPPL_CONTENT_PHOTO;
  else if (!strcmp(informat, "image/png"))        // Screen shots
    content_type = PAPPL_CONTENT_GRAPHIC;
  else if (!strcmp(informat, "application/pdf"))  // PDF, creating app in
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
	  p ++;
	for (i = 0; fields[i]; i ++)
	  if (strncasecmp(p, fields[i], strlen(fields[i])) == 0 &&
	      (isspace(*(p + strlen(fields[i]))) ||
	       *(p + strlen(fields[i])) == ':'))
	    break;
	if (fields[i])
	{
	  p += strlen(fields[i]);
	  while (isspace(*p))
	    p ++;
	  if (*p == ':')
	  {
	    p ++;
	    while (isspace(*p))
	      p ++;
	    while ((q = p + strlen(p) - 1) && (*q == '\n' || *q == '\r'))
	      *q = '\0';
	    papplLogJob(job, PAPPL_LOGLEVEL_DEBUG,
			"PDF metadata line: %s: %s", fields[i], p);
	    creatorline_found = 1;
	    for (j = 0; j < 5; j ++)
	    {
	      for (k = 0; creating_apps[j][k]; k ++)
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
	      (content_type == PAPPL_CONTENT_AUTO ? "No optimization" :
	       (content_type == PAPPL_CONTENT_PHOTO ? "Photo" :
		(content_type == PAPPL_CONTENT_GRAPHIC ? "Graphics" :
		 (content_type == PAPPL_CONTENT_TEXT ? "Text" :
		  "Text and graphics")))));

  return (content_type);
}


//
// '_prJobIsCanceled()' - Return 1 if the job is canceled, which is
//                        the case when papplJobIsCanceled() returns
//                        true.
//

int
_brfJobIsCanceled(void *data)
{
  pappl_job_t *job = (pappl_job_t *)data;


  return (papplJobIsCanceled(job) ? 1 : 0);
}


//
// '_prJobLog()' - Job log function which calls
//                 papplJobSetImpressionsCompleted() on page logs of
//                 filter functions
//

void
_brfJobLog(void *data,
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