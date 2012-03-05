/**
 *  @example oglplus/026_furry_torus.cpp
 *  @brief Shows simple simulation of fur
 *
 *  @image html 026_furry_torus.png
 *
 *  Copyright 2008-2012 Matus Chochlik. Distributed under the Boost
 *  Software License, Version 1.0. (See accompanying file
 *  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
#include <oglplus/gl.hpp>
#include <oglplus/all.hpp>

#include <oglplus/bound/texture.hpp>

#include <oglplus/images/load.hpp>
#include <oglplus/shapes/torus.hpp>
#include <oglplus/shapes/wrapper.hpp>

#include <cstdlib>

#include "example.hpp"

namespace oglplus {


class FurVertShader
 : public VertexShader
{
public:
	FurVertShader(void)
	 : VertexShader(
		"Fur vertex shader",
		"#version 330\n"
		"uniform mat4 NewModelMatrix, OldModelMatrix;"
		"uniform sampler2D FurTex;"

		"in vec4 Position;"
		"in vec3 Normal;"
		"in vec2 TexCoord;"

		"out vec3 vertOffset;"
		"out vec3 vertNormal;"
		"out vec3 vertColor;"

		"void main(void)"
		"{"
		"	gl_Position = "
		"		NewModelMatrix * "
		"		Position;"
		"	vertOffset = ("
		"		gl_Position - "
		"		OldModelMatrix * "
		"		Position"
		"	).xyz;"
		"	vertNormal = ("
		"		NewModelMatrix * "
		"		vec4(Normal, 0.0)"
		"	).xyz;"
		"	vertColor = texture(FurTex, TexCoord).rgb;"
		"}"
	)
	{ }
};

class FurGeomShader
 : public GeometryShader
{
public:
	FurGeomShader(void)
	 : GeometryShader(
		"Fur geometry shader",
		"#version 330\n"
		"#define PointCount 4\n"
		"layout(points) in;"
		"layout(line_strip, max_vertices = PointCount) out;"

		"uniform vec3 LightPosition;"
		"uniform mat4 CameraMatrix;"
		"uniform float Time;"
		"const float FurLength = 0.45;"
		"const float SegPart = 1.0 / (PointCount - 1);"
		"const float SegLen = FurLength * SegPart;"

		"in vec3 vertOffset[];"
		"in vec3 vertNormal[];"
		"in vec3 vertColor[];"

		"out vec3 geomNormal;"
		"out vec3 geomFurDir;"
		"out vec3 geomLightDir;"
		"out vec3 geomColor;"
		"out float geomFurPart;"
		"void main(void)"
		"{"
		"	geomColor = vertColor[0];"
		"	geomNormal = vertNormal[0];"
		"	geomFurPart = 0.0;"
		"	vec4 VertPos = gl_in[0].gl_Position;"
		"	float Wind = sin(4.0*(VertPos.x+Time));"

		"	for(int i=0; i!=PointCount; ++i)"
		"	{"
		"		geomLightDir = normalize(LightPosition - VertPos.xyz);"
		"		geomFurDir = normalize("
		"			vertNormal[0] * 0.1 - "
		"			vertOffset[0] * i*i*0.3 + "
		"			vec3(Wind*i*i*0.01, 0.0, 0.0)"
		"		);"
		"		gl_Position = CameraMatrix * VertPos;"
		"		EmitVertex();"
		"		geomFurPart += SegPart;"
		"		VertPos += vec4(geomFurDir, 0.0) * SegLen;"
		"	}"
		"	EndPrimitive();"
		"}"
	)
	{ }
};

class FurFragShader
 : public FragmentShader
{
public:
	FurFragShader(void)
	 : FragmentShader(
		"Fur fragment shader",
		"#version 330\n"

		"in vec3 geomNormal;"
		"in vec3 geomFurDir;"
		"in vec3 geomLightDir;"
		"in vec3 geomColor;"
		"in float geomFurPart;"

		"out vec3 fragColor;"
		"const vec3 LightColor = vec3(1.0, 1.0, 1.0);"

		"void main(void)"
		"{"
		"	const float Ambient = 0.3;"
		"	float FurLight = 1.0 - abs(dot(geomFurDir, geomLightDir));"
		"	float ShapeLight = max(dot(geomNormal, geomLightDir)+0.4, 0.0);"
		"	float Diffuse = FurLight * ShapeLight;"
		"	vec3 Color = mix(vec3(0.2, 0.2, 0.2), geomColor, geomFurPart);"
		"	fragColor = "
		"		Ambient * Color +"
		"		sqrt(Diffuse) * geomFurPart * Color;"
		"}"
	)
	{ }
};

class FurProgram
 : public HardwiredProgram<FurVertShader, FurGeomShader, FurFragShader>
{
private:
	const Program& prog(void) const { return *this; }
public:
	ProgramUniform<Mat4f> camera_matrix, new_model_matrix, old_model_matrix;
	ProgramUniform<Vec3f> light_position;
	ProgramUniform<GLfloat> time;
	ProgramUniformSampler fur_tex;

	FurProgram(void)
	 : HardwiredProgram<FurVertShader, FurGeomShader, FurFragShader>("Fur")
	 , camera_matrix(prog(), "CameraMatrix")
	 , new_model_matrix(prog(), "NewModelMatrix")
	 , old_model_matrix(prog(), "OldModelMatrix")
	 , light_position(prog(), "LightPosition")
	 , time(prog(), "Time")
	 , fur_tex(prog(), "FurTex")
	{ }
};

class Fur
{
protected:
	Context gl;

	VertexArray vao;
	Array<Buffer> vbos;

	const size_t hair_count;

public:
	Fur(const Program& prog)
	 : vbos(3)
	 , hair_count(96*1024)
	{
		// bind the VAO for the shape
		vao.Bind();


		std::vector<GLfloat> pos(hair_count * 3);
		std::vector<GLfloat> nms(hair_count * 3);
		std::vector<GLfloat> tcs(hair_count * 2);

		size_t i = 0, j = 0, k = 0;

		for(size_t h=0; h!=hair_count; ++h)
		{
			GLfloat u = GLfloat(std::rand()) / RAND_MAX;
			GLfloat z = GLfloat(std::rand()) / RAND_MAX;
			GLfloat v = z + std::pow(std::sin(2.0*M_PI*z)/(4.0*M_PI), 3);

			auto phi = FullCircles(u);
			auto rho = FullCircles(v);

			pos[i++] = Cos(phi) * (0.5 + 0.5 * (1.0 + Cos(rho)));
			pos[i++] = Sin(rho) * 0.5;
			pos[i++] = Sin(phi) * (0.5 + 0.5 * (1.0 + Cos(rho)));

			nms[j++] = Cos(phi) * Cos(rho);
			nms[j++] = Sin(rho);
			nms[j++] = Sin(phi) * Cos(rho);

			tcs[k++] = u * 4.0;
			tcs[k++] = v * 2.0;
		}

		vbos[0].Bind(Buffer::Target::Array);
		{
			Buffer::Data(Buffer::Target::Array, pos);

			VertexAttribArray attr(prog, "Position");
			attr.Setup(3, DataType::Float);
			attr.Enable();
		}

		vbos[1].Bind(Buffer::Target::Array);
		{
			Buffer::Data(Buffer::Target::Array, nms);

			VertexAttribArray attr(prog, "Normal");
			attr.Setup(3, DataType::Float);
			attr.Enable();
		}

		vbos[2].Bind(Buffer::Target::Array);
		{
			Buffer::Data(Buffer::Target::Array, tcs);

			VertexAttribArray attr(prog, "TexCoord");
			attr.Setup(2, DataType::Float);
			attr.Enable();
		}
	}

	void Draw(void)
	{
		vao.Bind();
		gl.DrawArrays(PrimitiveType::Points, 0, hair_count);
	}
};

class TorusVertShader
 : public VertexShader
{
public:
	TorusVertShader(void)
	 : VertexShader(
		"Torus vertex shader",
		"#version 330\n"
		"uniform mat4 CameraMatrix, ModelMatrix;"

		"in vec4 Position;"

		"void main(void)"
		"{"
		"	gl_Position = CameraMatrix * ModelMatrix * Position;"
		"}"
	)
	{ }
};

class TorusFragShader
 : public FragmentShader
{
public:
	TorusFragShader(void)
	 : FragmentShader(
		"Torus fragment shader",
		"#version 330\n"

		"out vec3 fragColor;"

		"void main(void)"
		"{"
		"	fragColor = vec3(0.05, 0.05, 0.01);"
		"}"
	)
	{ }
};

class TorusProgram
 : public HardwiredProgram<TorusVertShader, TorusFragShader>
{
private:
	const Program& prog(void) const { return *this; }
public:
	ProgramUniform<Mat4f> camera_matrix, model_matrix;

	TorusProgram(void)
	 : HardwiredProgram<TorusVertShader, TorusFragShader>("Torus")
	 , camera_matrix(prog(), "CameraMatrix")
	 , model_matrix(prog(), "ModelMatrix")
	{ }
};

typedef shapes::ShapeWrapper<shapes::Torus> Torus;

class FurExample : public Example
{
private:
	Context gl;

	TorusProgram torus_prog;
	FurProgram fur_prog;

	Torus torus;
	Fur fur;

	double prev_time, accel, prev_vel, curr_vel;

	Mat4f projection;

	Texture fur_tex;
public:
	FurExample(void)
	 : torus({"Position"}, shapes::Torus(), torus_prog)
	 , fur(fur_prog)
	 , prev_time(0.0)
	 , accel(0.0)
	 , prev_vel(0.0)
	 , curr_vel(0.0)
	{
		Texture::Active(0);
		{
			auto bound_tex = Bind(fur_tex, Texture::Target::_2D);
			bound_tex.Image2D(images::LoadTexture("zebra_fur"));
			bound_tex.GenerateMipmap();
			bound_tex.MinFilter(TextureMinFilter::LinearMipmapLinear);
			bound_tex.MagFilter(TextureMagFilter::Linear);
			bound_tex.WrapS(TextureWrap::Repeat);
			bound_tex.WrapT(TextureWrap::Repeat);
		}
		fur_prog.fur_tex = 0;
		fur_prog.light_position = Vec3f(5.0, 6.0, 4.0);

		gl.ClearColor(0.5f, 0.5f, 0.4f, 0.0f);
		gl.ClearDepth(1.0f);
		gl.Enable(Capability::DepthTest);
		gl.Enable(Capability::LineSmooth);
		gl.LineWidth(2.0);
	}

	void SetModelMatrix(ProgramUniform<Mat4f>& matrix, double value)
	{
		matrix = ModelMatrixf::RotationZ(FullCircles(value / 4.0));
	}

	void Reshape(size_t width, size_t height)
	{
		gl.Viewport(width, height);
		projection = CamMatrixf::Perspective(
			Degrees(48),
			double(width)/height,
			1, 100
		);
	}

	void Render(double time)
	{
		gl.Clear().ColorBuffer().DepthBuffer();

		const CamMatrixf camera = CamMatrixf::Orbiting(
			Vec3f(),
			4.5,
			FullCircles(time / 21.0),
			Degrees(SineWave(time / 15.0) * 80)
		);

		switch(int(0.25*time) % 8)
		{
			case 0: accel += (time - prev_time)*0.01; break;
			case 4: accel -= (time - prev_time)*0.01; break;
		}
		curr_vel += accel;

		Mat4f curr_model = ModelMatrixf::RotationZ(FullCircles(curr_vel / 4.0));
		Mat4f prev_model = ModelMatrixf::RotationZ(FullCircles(prev_vel / 4.0));

		fur_prog.old_model_matrix = prev_model;
		fur_prog.new_model_matrix = curr_model;

		prev_vel += (curr_vel - prev_vel) * 0.5;
		prev_time = time;

		torus_prog.model_matrix = curr_model;
		torus_prog.camera_matrix = projection * camera;
		torus_prog.Use();
		torus.Use();
		torus.Draw();

		fur_prog.camera_matrix = projection * camera;
		fur_prog.time = time;
		fur_prog.Use();
		fur.Draw();
	}

	bool Continue(double time)
	{
		return time < 90.0;
	}

	double ScreenshotTime(void) const
	{
		return 2.0;
	}
};

std::unique_ptr<Example> makeExample(const ExampleParams& params)
{
	return std::unique_ptr<Example>(new FurExample);
}

} // namespace oglplus
