#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <unistd.h>

#include "bcm_host.h"

#include "utils.h"

#include "GLES2/gl2.h"
#include "EGL/egl.h"
#include "EGL/eglext.h"

typedef struct {
   uint32_t screen_width;
   uint32_t screen_height;
// OpenGL|ES objects
   EGLDisplay display;
   EGLSurface surface;
   EGLContext context;

   GLuint verbose;
   GLuint vshader;
   GLuint fshader;
   GLuint program;
   GLuint buf;

   GLuint attr_pos; 

} CUBE_STATE_T;

static CUBE_STATE_T _state, *state=&_state;

#define check() assert(glGetError() == 0)

static void showlog(GLint shader){
   // Prints the compile log for a shader
   char log[1024];
   glGetShaderInfoLog(shader,sizeof log,NULL,log);
   printf("%d:shader:\n%s\n", shader, log);
}

static void showprogramlog(GLint shader){
   // Prints the information log for a program object
   char log[1024];
   glGetProgramInfoLog(shader,sizeof log,NULL,log);
   printf("%d:program:\n%s\n", shader, log);
}

static void init_ogl(CUBE_STATE_T *state){
   int32_t success = 0;
   EGLBoolean result;
   EGLint num_config;

   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] = {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };
   
   static const EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   EGLConfig config;

   // get an EGL display connection
   state->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   assert(state->display!=EGL_NO_DISPLAY);
   check();

   // initialize the EGL display connection
   result = eglInitialize(state->display, NULL, NULL);
   assert(EGL_FALSE != result);
   check();

   // get an appropriate EGL frame buffer configuration
   result = eglChooseConfig(state->display, attribute_list, &config, 1, &num_config);
   assert(EGL_FALSE != result);
   check();

   // get an appropriate EGL frame buffer configuration
   result = eglBindAPI(EGL_OPENGL_ES_API);
   assert(EGL_FALSE != result);
   check();

   // create an EGL rendering context
   state->context = eglCreateContext(state->display, config, EGL_NO_CONTEXT, context_attributes);
   assert(state->context!=EGL_NO_CONTEXT);
   check();

   // create an EGL window surface
   success = graphics_get_display_size(0 /* LCD */, &state->screen_width, &state->screen_height);
   assert( success >= 0 );

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = state->screen_width;
   dst_rect.height = state->screen_height;
      
   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = state->screen_width << 16;
   src_rect.height = state->screen_height << 16;        

   dispman_display = vc_dispmanx_display_open( 0 /* LCD */);
   dispman_update = vc_dispmanx_update_start( 0 );
         
   dispman_element = vc_dispmanx_element_add ( dispman_update, dispman_display,
      0/*layer*/, &dst_rect, 0/*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, 0 /*alpha*/, 0/*clamp*/, 0/*transform*/);
      
   nativewindow.element = dispman_element;
   nativewindow.width = state->screen_width;
   nativewindow.height = state->screen_height;
   vc_dispmanx_update_submit_sync( dispman_update );
      
   check();

   state->surface = eglCreateWindowSurface( state->display, config, &nativewindow, NULL );
   assert(state->surface != EGL_NO_SURFACE);
   check();

   // connect the context to the surface
   result = eglMakeCurrent(state->display, state->surface, state->surface, state->context);
   assert(EGL_FALSE != result);
   check();

   // Set background color and clear buffers
   glClearColor(0.15f, 0.25f, 0.35f, 1.0f);
   glClear( GL_COLOR_BUFFER_BIT );

   check();
}


static void init_shaders(CUBE_STATE_T *state) {
   static const GLfloat vertex_data[] = {
        -1.0,-1.0,1.0,1.0,
        1.0,-1.0,1.0,1.0,
        1.0,1.0,1.0,1.0,
        -1.0,1.0,1.0,1.0
   };
   const GLchar *vshader_source =
              "attribute vec4 a_position;"
              "varying vec2 v_texcoord;"
              "void main(void) {"
              "    gl_Position = a_position;"
              "    v_texcoord = a_position.xy*0.5+0.5;"
              "}";
      
  std::string fragSource;

  if(!loadFromPath("test.frag", &fragSource)) {
      return;
    }

    const GLchar *fshader_source = (const GLchar*) fragSource.c_str();

        state->vshader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(state->vshader, 1, &vshader_source, 0);
        glCompileShader(state->vshader);
        check();

        if (state->verbose)
            showlog(state->vshader);
            
        state->fshader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(state->fshader, 1, &fshader_source, 0);
        glCompileShader(state->fshader);
        check();

        if (state->verbose)
            showlog(state->fshader);

        state->program = glCreateProgram();
        glAttachShader(state->program, state->vshader);
        glAttachShader(state->program, state->fshader);
        glLinkProgram(state->program);
        check();

        if (state->verbose)
            showprogramlog(state->program);
            
        state->attr_pos = glGetAttribLocation(state->program, "a_position");
        check();
   
        glClearColor ( 0.0, 1.0, 1.0, 1.0 );
        
        glGenBuffers(1, &state->buf);

        check();

        // Prepare viewport
        glViewport ( 0, 0, state->screen_width, state->screen_height );
        check();
        
        // Upload vertex data to a buffer
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_data),
                             vertex_data, GL_STATIC_DRAW);
        glVertexAttribPointer(state->attr_pos, 4, GL_FLOAT, 0, 16, 0);
        glEnableVertexAttribArray(state->attr_pos);
        check();
}
        
static void draw(CUBE_STATE_T *state, GLfloat cx, GLfloat cy){
        // Now render to the main frame buffer
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        // Clear the background (not really necessary I suppose)
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        check();
        
        glBindBuffer(GL_ARRAY_BUFFER, state->buf);
        check();
        glUseProgram ( state->program );
        check();
        // glUniform4f(state->unif_color, 0.5, 0.5, 0.8, 1.0);
        // glUniform2f(state->unif_scale, scale, scale);
        // glUniform1i(state->unif_tex, 0); // I don't really understand this part, perhaps it relates to active texture?
        // check();
        
        glDrawArrays ( GL_TRIANGLE_FAN, 0, 4 );
        check();

        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glFlush();
        glFinish();
        check();
        
        eglSwapBuffers(state->display, state->surface);
        check();
}

static int get_mouse(CUBE_STATE_T *state, int *outx, int *outy){
    static int fd = -1;
    const int width=state->screen_width, height=state->screen_height;
    static int x=800, y=400;
    const int XSIGN = 1<<4, YSIGN = 1<<5;
    if (fd<0) {
       fd = open("/dev/input/mouse0",O_RDONLY|O_NONBLOCK);
    }
    if (fd>=0) {
        struct {char buttons, dx, dy; } m;
        while (1) {
           int bytes = read(fd, &m, sizeof m);
           if (bytes < (int)sizeof m) goto _exit;
           if (m.buttons&8) {
              break; // This bit should always be set
           }
           read(fd, &m, 1); // Try to sync up again
        }
        if (m.buttons&3)
           return m.buttons&3;
        x+=m.dx;
        y+=m.dy;
        if (m.buttons&XSIGN)
           x-=256;
        if (m.buttons&YSIGN)
           y-=256;
        if (x<0) x=0;
        if (y<0) y=0;
        if (x>width) x=width;
        if (y>height) y=height;
   }
_exit:
   if (outx) *outx = x;
   if (outy) *outy = y;
   return 0;
}       
 
//==============================================================================

int main (){
   int terminate = 0;
   GLfloat cx, cy;
   bcm_host_init();

   // Clear application state
   memset( state, 0, sizeof( *state ) );
      
   // Start OGLES
   init_ogl(state);
   init_shaders(state);
   cx = state->screen_width/2;
   cy = state->screen_height/2;

   while (!terminate){
      int x, y, b;
      b = get_mouse(state, &x, &y);
      if (b) break;
      draw(state, x, y);
   }
   return 0;
}
