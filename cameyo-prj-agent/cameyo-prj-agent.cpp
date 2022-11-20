// cameyo-prj-agent.cpp : Defines the exported functions for the DLL.
//

#include "pch.h"
#include "framework.h"
#include "cameyo-prj-agent.h"


// This is an example of an exported variable
CAMEYOPRJAGENT_API int ncameyoprjagent=0;

// This is an example of an exported function.
CAMEYOPRJAGENT_API int fncameyoprjagent(void)
{
    return 0;
}

// This is the constructor of a class that has been exported.
Ccameyoprjagent::Ccameyoprjagent()
{
    return;
}
