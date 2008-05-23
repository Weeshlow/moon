// define this here so that protypes.h isn't included (and doesn't
// muck with our npapi.h)
#define NO_NSPR_10_SUPPORT

#include "plugin.h"

#include "ff2-bridge.h"

#define CONCAT(x,y) x##y
#define GECKO_SYM(x) CONCAT(FF2,x)
#include "../browser-http.inc"

BrowserRequest*
FF2BrowserBridge::CreateBrowserRequest (const char *method, const char *uri)
{
	return new FF2BrowserRequest (method, uri);
}
