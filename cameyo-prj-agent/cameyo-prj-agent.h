// The following ifdef block is the standard way of creating macros which make exporting
// from a DLL simpler. All files within this DLL are compiled with the CAMEYOPRJAGENT_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see
// CAMEYOPRJAGENT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef CAMEYOPRJAGENT_EXPORTS
#define CAMEYOPRJAGENT_API __declspec(dllexport)
#else
#define CAMEYOPRJAGENT_API __declspec(dllimport)
#endif

// This class is exported from the dll
class CAMEYOPRJAGENT_API Ccameyoprjagent {
public:
	Ccameyoprjagent(void);
	// TODO: add your methods here.
};

extern CAMEYOPRJAGENT_API int ncameyoprjagent;

CAMEYOPRJAGENT_API int fncameyoprjagent(void);
