#ifndef OPENGL_SHARED_HPP
#define OPENGL_SHARED_HPP

#include <stdlib.h>
#if defined(_WIN32)
#include <windows.h>
#include <gl/GLee.h>
#include <gl/gl.h>
#include <gl/glu.h>
#else
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glu.h>
#endif

namespace JUI
{

	/*
	 * Generic function used by OpenGL.
	 */
	class OpenGLShared
	{

	public:

		static GLsizei next_power_of_2( GLsizei num );

	};

}

#endif // OPENGL_SHARED_HPP
