This is a Virtual Terminal environment that aims for minimalism and targets UNIX like systems.
  
Components:
  
  - libvwm:  
    This is an implementation of a tiny virtual terminal window manager library (included also a sample    
    application that initialize the library). See at src/libvwm for details.  
  
  - libvtach:  
    This is a library that extends libvwm by linking against, with attaching|detaching   
    capabilities (included also a sample application). See at src/libvtach for details.  
  
  - libvwmed:  
    This is a library that extends libvwm with readline and editor capabilities.  
    It links against libved which is available in this repository.  
    It includes also an application that initialize the library). See at src/libvwmed for details.  
