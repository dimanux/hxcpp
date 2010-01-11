#ifndef HX_CFFI_LOADER_H
#define HX_CFFI_LOADER_H


typedef void *(*ResolveProc)(const char *inName);
static ResolveProc sResolveProc = 0;


#ifdef STATIC_LINK
extern "C" void * hx_cffi(const char *inName);

#define LoadFunc hx_cffi

#else  // Dynamic link


#ifdef NEKO_WINDOWS

#include <windows.h>
#include <stdio.h>
 
void *LoadFunc(const char *inName)
{
   static char *modules[] = { 0, "hxcpp", "hxcpp-debug" };
   for(int i=0; i<3 && sResolveProc==0; i++)
   {
      HMODULE handle = GetModuleHandle(modules[i]);
      if (handle)
      {
         sResolveProc = (ResolveProc)GetProcAddress(handle,"hx_cffi");
         if (sResolveProc==0)
            FreeLibrary(handle);
      }
   }
   if (sResolveProc==0)
   {
      // Maybe we are part of a neko script?
      HMODULE handle = LoadLibrary("nekoapi.dll");
      if (handle)
      {
         sResolveProc = (ResolveProc)GetProcAddress(handle,"hx_cffi");
         if (sResolveProc==0)
            FreeLibrary(handle);
      }
   }
   if (sResolveProc==0)
   {
      fprintf(stderr,"Could not link plugin to process (hxCFFILoader.h %d)\n",__LINE__);
      exit(1);
   }
   return sResolveProc(inName);
}

#else // not windows

#ifdef NEKO_LINUX
#define EXT "dso"
//#define __USE_GNU 1
#else
#include <mach-o/dyld.h>
#define EXT "dylib"
#endif

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

void *LoadFunc(const char *inName)
{
   if (sResolveProc==0)
   {
      sResolveProc = (ResolveProc)dlsym(RTLD_DEFAULT,"hx_cffi");
   }
   if (sResolveProc==0)
   {
      void *handle = dlopen("nekoapi." EXT ,RTLD_NOW);
      if (handle)
         sResolveProc = (ResolveProc)dlsym(handle,"hx_cffi");
   }
   if (sResolveProc==0)
   {
      bool debug = getenv("HXCPP_LOAD_DEBUG");
#ifndef NEKO_LINUX
      int count = _dyld_image_count();
      for(int i=0;i<count;i++)
      {
         const char *s =  _dyld_get_image_name(i);
         char *buf = (char *)malloc( strlen(s) + 20 );
         strcpy(buf,s);
         char *slash = rindex(buf,'/');
         if (slash)
         {
            slash[1] = '\0';
            strcat(slash, "nekoapi." EXT );
            if (debug)
               printf(" -> %s\n", buf );
            void *handle = dlopen(buf, RTLD_NOW);
            if (handle)
            {
               sResolveProc = (ResolveProc)dlsym(handle,"hx_cffi");
               free(buf);
               break;
            }
         }
         free(buf);
      }
#else
      // Find module for this function ...
      Dl_info info;
      if (dladdr((void *)LoadFunc,&info))
      {
         if (debug)
            printf("Found loaded module : %s\n", info.dli_fname );

         char *buf = (char *)malloc( strlen(info.dli_fname) + 20 );
         strcpy(buf,info.dli_fname);
         char *slash = rindex(buf,'/');
         if (slash)
         {
            slash[1] = '\0';
            strcat(slash, "nekoapi." EXT );
            if (debug)
               printf(" -> %s\n", buf );
            void *handle = dlopen(buf, RTLD_NOW);
            if (handle)
               sResolveProc = (ResolveProc)dlsym(handle,"hx_cffi");
         }
         free(buf);
      }
      else if (debug)
         printf("Could not find loaded module?\n");
#endif
   }
   if (sResolveProc==0)
   {
      fprintf(stderr,"Could not link plugin to process (hxCFFILoader.h %d)\n",__LINE__);
      exit(1);
   }
   return sResolveProc(inName);
}

#undef EXT

#endif

#endif // not static link
 
#define DEFFUNC(name,ret,def_args,call_args) \
typedef ret (*FUNC_##name)def_args; \
extern FUNC_##name name; \
ret IMPL_##name def_args \
{ \
   name = (FUNC_##name)LoadFunc(#name); \
   return name call_args; \
}\
FUNC_##name name = IMPL_##name;
 



#endif