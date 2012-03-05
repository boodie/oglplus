/**
 *  @example oglplus/027_depth_of_field.cpp
 *  @brief Shows simple simulation of the Depth-of-field effect
 *
 *  @image html 027_depth_of_field.png
 *
 *  Copyright 2008-2012 Matus Chochlik. Distributed under the Boost
 *  Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#include <oglplus/gl.hpp>
#include <oglplus/all.hpp>
#include <oglplus/shapes/cube.hpp>

#include <oglplus/bound/texture.hpp>
#include <oglplus/bound/framebuffer.hpp>

#include <cmath>

#include "example.hpp"

namespace oglplus {

class DOFExample : public Example
{
private:
	// helper object building cube vertex attributes
	shapes::Cube make_cube;
	// helper object encapsulating cube drawing instructions
	shapes::DrawingInstructions face_instr, edge_instr;
	// indices pointing to cube primitive elements
	shapes::Cube::IndexArray face_indices, edge_indices;

	std::vector<Mat4f> cube_matrices;

	// Returns a vector of cube offsets
	static std::vector<Mat4f> MakeCubeMatrices(size_t count, float max_dist)
	{
		std::srand(59039);
		std::vector<Mat4f> offsets(count);
		for(size_t i=0; i!=count; ++i)
		{
			float x = float(std::rand())/RAND_MAX;
			float y = float(std::rand())/RAND_MAX;
			float z = float(std::rand())/RAND_MAX;
			float sx = std::rand()%2 ? 1.0f: -1.0f;
			float sy = std::rand()%2 ? 1.0f: -1.0f;
			float sz = std::rand()%2 ? 1.0f: -1.0f;
			offsets[i] =
				ModelMatrixf::Translation(
					sx*(1.0f + std::pow(x, 0.9) * max_dist),
					sy*(1.0f + std::pow(y, 1.5) * max_dist),
					sz*(1.0f + std::pow(z, 0.7) * max_dist)
				) *
				ModelMatrixf::RotationZ(
					RightAngles(float(std::rand())/RAND_MAX)
				) *
				ModelMatrixf::RotationY(
					RightAngles(float(std::rand())/RAND_MAX)
				) *
				ModelMatrixf::RotationX(
					RightAngles(float(std::rand())/RAND_MAX)
				);
		}
		return std::move(offsets);
	}

	// wrapper around the current OpenGL context
	Context gl;

	// Vertex shader
	VertexShader main_vs, dof_vs;

	// Fragment shader
	FragmentShader main_fs, dof_fs;

	// Program
	Program main_prog, dof_prog;

	// A vertex array object for the rendered cube and the postproc screen
	VertexArray cube, screen;

	// VBOs for the cube's and screens vertex attribs
	Buffer positions, normals, corners;

	// The framebuffer object of offscreen rendering
	Framebuffer fbo;

	// The color and depth textures
	Texture color_tex, depth_tex;

	size_t width, height;
public:
	DOFExample(void)
	 : face_instr(make_cube.Instructions())
	 , edge_instr(make_cube.EdgeInstructions())
	 , face_indices(make_cube.Indices())
	 , edge_indices(make_cube.EdgeIndices())
	 , cube_matrices(MakeCubeMatrices(100, 10.0))
	 , width(800)
	 , height(600)
	{
		main_vs.Source(
			"#version 330\n"
			"uniform mat4 ProjectionMatrix, CameraMatrix, ModelMatrix;"
			"uniform vec3 LightPos;"
			"in vec4 Position;"
			"in vec3 Normal;"
			"out vec3 vertLightDir;"
			"out vec3 vertNormal;"
			"void main(void)"
			"{"
			"	gl_Position = "
			"		ProjectionMatrix *"
			"		CameraMatrix *"
			"		ModelMatrix *"
			"		Position;"
			"	vertLightDir = normalize(("
			"		vec4(LightPos, 0.0) - "
			"		ModelMatrix *"
			"		Position"
			"	).xyz);"
			"	vertNormal = normalize(("
			"		ModelMatrix *"
			"		vec4(Normal, 0.0)"
			"	).xyz);"
			"}"
		);
		// compile it
		main_vs.Compile();

		// set the fragment shader source
		main_fs.Source(
			"#version 330\n"
			"uniform vec3 AmbientColor, DiffuseColor;"
			"in vec3 vertLightDir;"
			"in vec3 vertNormal;"
			"out vec4 fragColor;"
			"void main(void)"
			"{"
			"	float d = max(dot(vertLightDir,vertNormal),0.0);"
			"	float e = sin("
			"		10.0*vertLightDir.x + "
			"		20.0*vertLightDir.y + "
			"		25.0*vertLightDir.z   "
			"	)*0.9;"
			"	fragColor = vec4("
			"		mix(AmbientColor, DiffuseColor, d+e),"
			"		1.0"
			"	);"
			"}"
		);
		// compile it
		main_fs.Compile();

		// attach the shaders to the program
		main_prog.AttachShader(main_vs);
		main_prog.AttachShader(main_fs);
		// link and use it
		main_prog.Link();
		main_prog.Use();

		// bind the VAO for the cube
		cube.Bind();

		// bind the VBO for the cube vertices
		positions.Bind(Buffer::Target::Array);
		{
			std::vector<GLfloat> data;
			GLuint n_per_vertex = make_cube.Positions(data);
			// upload the data
			Buffer::Data(Buffer::Target::Array, data);
			// setup the vertex attribs array for the vertices
			VertexAttribArray attr(main_prog, "Position");
			attr.Setup(n_per_vertex, DataType::Float);
			attr.Enable();
		}

		// bind the VBO for the cube normals
		normals.Bind(Buffer::Target::Array);
		{
			std::vector<GLfloat> data;
			GLuint n_per_vertex = make_cube.Normals(data);
			Buffer::Data(Buffer::Target::Array, data);
			VertexAttribArray attr(main_prog, "Normal");
			attr.Setup(n_per_vertex, DataType::Float);
			attr.Enable();
		}

		Uniform<Vec3f>(main_prog, "LightPos").Set(30.0, 50.0, 20.0);

		dof_vs.Source(
			"#version 330\n"
			"in vec4 Position;"
			"out vec2 vertTexCoord;"
			"void main(void)"
			"{"
			"	gl_Position = Position;"
			"	vertTexCoord = vec2("
			"		(Position.x*0.5 + 0.5)*800,"
			"		(Position.y*0.5 + 0.5)*600 "
			"	);"
			"}"
		);
		dof_vs.Compile();

		dof_fs.Source(
			"#version 330\n"
			"uniform sampler2DRect ColorTex;"
			"uniform sampler2DRect DepthTex;"
			"uniform float FocusDepth;"
			"in vec2 vertTexCoord;"
			"out vec4 fragColor;"
			"const float strength = 32.0;"
			"void main(void)"
			"{"
			"	float fragDepth = texture(DepthTex, vertTexCoord).r;"
			"	vec3 color = texture(ColorTex, vertTexCoord).rgb;"
			"	float of = abs(fragDepth - FocusDepth);"
			"	int nsam = int(of*128);"
			"	float inv_nsam = 1.0 / (1.0 + nsam);"
			"	float astep = (3.14151*4.0)/nsam;"
			"	for(int i=0; i!=nsam; ++i)"
			"	{"
			"		float a = i*astep;"
			"		float d = sqrt(i*inv_nsam);"
			"		float sx = cos(a)*of*strength*d;"
			"		float sy = sin(a)*of*strength*d;"
			"		vec2 samTexCoord = vertTexCoord + vec2(sx, sy) + noise2(vec2(sx, sy));"
			"		color += texture(ColorTex, samTexCoord).rgb;"
			"	}"
			"	fragColor = vec4(color * inv_nsam , 1.0);"
			"}"
		);
		dof_fs.Compile();

		dof_prog.AttachShader(dof_vs);
		dof_prog.AttachShader(dof_fs);
		dof_prog.Link();
		dof_prog.Use();

		// bind the VAO for the screen
		screen.Bind();

		corners.Bind(Buffer::Target::Array);
		{
			GLfloat screen_verts[8] = {
				-1.0f, -1.0f,
				-1.0f,  1.0f,
				 1.0f, -1.0f,
				 1.0f,  1.0f
			};
			Buffer::Data(Buffer::Target::Array, 8, screen_verts);
			VertexAttribArray attr(dof_prog, "Position");
			attr.Setup(2, DataType::Float);
			attr.Enable();
		}

		Texture::Active(0);
		UniformSampler(dof_prog, "ColorTex").Set(0);
		{
			auto bound_tex = Bind(color_tex, Texture::Target::Rectangle);
			bound_tex.MinFilter(TextureMinFilter::Linear);
			bound_tex.MagFilter(TextureMagFilter::Linear);
			bound_tex.WrapS(TextureWrap::ClampToEdge);
			bound_tex.WrapT(TextureWrap::ClampToEdge);
			bound_tex.Image2D(
				0,
				PixelDataInternalFormat::RGB,
				width, height,
				0,
				PixelDataFormat::RGB,
				PixelDataType::UnsignedByte,
				nullptr
			);
		}

		Texture::Active(1);
		UniformSampler(dof_prog, "DepthTex").Set(1);
		{
			auto bound_tex = Bind(depth_tex, Texture::Target::Rectangle);
			bound_tex.MinFilter(TextureMinFilter::Linear);
			bound_tex.MagFilter(TextureMagFilter::Linear);
			bound_tex.WrapS(TextureWrap::ClampToEdge);
			bound_tex.WrapT(TextureWrap::ClampToEdge);
			bound_tex.Image2D(
				0,
				PixelDataInternalFormat::DepthComponent,
				width, height,
				0,
				PixelDataFormat::DepthComponent,
				PixelDataType::Float,
				nullptr
			);
		}

		{
			auto bound_fbo = Bind(
				fbo,
				Framebuffer::Target::Draw
			);
			bound_fbo.AttachTexture(
				FramebufferAttachment::Color,
				color_tex,
				0
			);
			bound_fbo.AttachTexture(
				FramebufferAttachment::Depth,
				depth_tex,
				0
			);
		}

		//
		gl.ClearColor(0.9f, 0.9f, 0.9f, 0.0f);
		gl.ClearDepth(1.0f);
		gl.Enable(Capability::DepthTest);
		gl.DepthFunc(CompareFn::LEqual);
		gl.Enable(Capability::LineSmooth);
		gl.BlendFunc(BlendFn::SrcAlpha, BlendFn::OneMinusSrcAlpha);
	}

	void Reshape(size_t vp_width, size_t vp_height)
	{
		width = vp_width;
		height = vp_height;

		ProgramUniform<Mat4f>(main_prog, "ProjectionMatrix").Set(
			CamMatrixf::Perspective(
				Degrees(30),
				double(width)/height,
				1.0, 5.0
			)
		);

		Bind(color_tex, Texture::Target::Rectangle).Image2D(
			0,
			PixelDataInternalFormat::RGB,
			width, height,
			0,
			PixelDataFormat::RGB,
			PixelDataType::UnsignedByte,
			nullptr
		);

		Bind(depth_tex, Texture::Target::Rectangle).Image2D(
			0,
			PixelDataInternalFormat::DepthComponent,
			width, height,
			0,
			PixelDataFormat::DepthComponent,
			PixelDataType::Float,
			nullptr
		);
	}

	void Render(double time)
	{
		fbo.Bind(Framebuffer::Target::Draw);

		gl.Viewport(width, height);
		gl.Clear().ColorBuffer().DepthBuffer();

		main_prog.Use();
		cube.Bind();

		Uniform<Mat4f>(main_prog, "CameraMatrix").Set(
			CamMatrixf::Orbiting(
				Vec3f(),
				18.5,
				FullCircles(time / 20.0),
				Degrees(SineWave(time / 25.0) * 30)
			)
		);

		auto i = cube_matrices.begin(), e = cube_matrices.end();
		while(i != e)
		{
			Uniform<Mat4f>(main_prog, "ModelMatrix").Set(*i);
			Uniform<Vec3f>(main_prog, "AmbientColor").Set(0.7f, 0.6f, 0.2f);
			Uniform<Vec3f>(main_prog, "DiffuseColor").Set(1.0f, 0.8f, 0.3f);
			face_instr.Draw(face_indices);

			Uniform<Vec3f>(main_prog, "AmbientColor").Set(0.1f, 0.1f, 0.1f);
			Uniform<Vec3f>(main_prog, "DiffuseColor").Set(0.3f, 0.3f, 0.3f);
			edge_instr.Draw(edge_indices);
			++i;
		}

		Framebuffer::BindDefault(Framebuffer::Target::Draw);

		gl.Viewport(width, height);
		gl.Clear().ColorBuffer().DepthBuffer();

		dof_prog.Use();
		screen.Bind();

		Uniform<GLfloat>(dof_prog, "FocusDepth").Set(
			0.6 + SineWave(time / 9.0)*0.3
		);

		gl.Enable(Capability::Blend);
		gl.DrawArrays(PrimitiveType::TriangleStrip, 0, 4);
		gl.Disable(Capability::Blend);
	}

	bool Continue(double time)
	{
		return time < 60.0;
	}

	double ScreenshotTime(void) const
	{
		return 5.0;
	}
};

std::unique_ptr<Example> makeExample(const ExampleParams& params)
{
	return std::unique_ptr<Example>(new DOFExample);
}

} // namespace oglplus
