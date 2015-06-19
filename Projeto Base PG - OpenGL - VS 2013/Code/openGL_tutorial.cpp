#include <windows.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>

#define KEY_ESCAPE 27
#define PI 3.14159265

using namespace std;

GLfloat angle, fAspect;
int luz = -1, objeto = 0, h = 0;


/************************************************************************
Window
************************************************************************/

typedef struct {
	int width;
	int height;
	char* title;

	float field_of_view_angle;
	float z_near;
	float z_far;
} glutWindow;



/***************************************************************************
OBJ Loading
***************************************************************************/

class Model_OBJ
{
public:
	Model_OBJ();
	float* Model_OBJ::calculateNormal(float* coord1, float* coord2, float* coord3);
	int Model_OBJ::Load(char *filename);	// Loads the model
	void Model_OBJ::Draw();					// Draws the model on the screen
	void Model_OBJ::Release();				// Release the model

	float* normals;							// Stores the normals
	float* Faces_Triangles;					// Stores the triangles
	float* vertexBuffer;					// Stores the points which make the object
	long TotalConnectedPoints;				// Stores the total number of connected verteces
	long TotalConnectedTriangles;			// Stores the total number of connected triangles

};


#define POINTS_PER_VERTEX 3
#define TOTAL_FLOATS_IN_TRIANGLE 9
using namespace std;



//////////////////////////////////////////////////////////////////////////////////////////////////////////////
const double plane_size = 30.0;                 // Extent of plane
const double plane_level = -10;               // Level (y-coord) of plane
GLdouble planepts[4][3] = {                    // Corners of "plane"
	{ -plane_size, plane_level, -plane_size },
	{ -plane_size, plane_level, plane_size },
	{ plane_size, plane_level, plane_size },
	{ plane_size, plane_level, -plane_size } };

GLdouble plane_eq[4];                          // Holds plane "equation"
//  in the form Ax+By+Cz=D
GLdouble shadowmat[16];                        // Shadow projection matrix

// makeshadowmatrix
// Create a matrix that will project the desired shadow.
// Is given the plane equation, in the form Ax+By+Cz=D.
// Is given light postion, in homogeneous form.
// Creates matrix in shadowmat.
void makeshadowmatrix(GLdouble shadowmat[16],
	const GLdouble plane_eq[4],
	const GLdouble lightpos[4])
{
	// Find dot product between light position vector and ground plane normal.
	GLdouble dot = plane_eq[0] * lightpos[0] +
		plane_eq[1] * lightpos[1] +
		plane_eq[2] * lightpos[2] +
		plane_eq[3] * lightpos[3];

	typedef GLdouble(*mat44)[4];
	mat44 sm = reinterpret_cast<mat44>(shadowmat);

	sm[0][0] = dot - lightpos[0] * plane_eq[0];
	sm[1][0] = -lightpos[0] * plane_eq[1];
	sm[2][0] = -lightpos[0] * plane_eq[2];
	sm[3][0] = -lightpos[0] * plane_eq[3];

	sm[0][1] = -lightpos[1] * plane_eq[0];
	sm[1][1] = dot - lightpos[1] * plane_eq[1];
	sm[2][1] = -lightpos[1] * plane_eq[2];
	sm[3][1] = -lightpos[1] * plane_eq[3];

	sm[0][2] = -lightpos[2] * plane_eq[0];
	sm[1][2] = -lightpos[2] * plane_eq[1];
	sm[2][2] = dot - lightpos[2] * plane_eq[2];
	sm[3][2] = -lightpos[2] * plane_eq[3];

	sm[0][3] = -lightpos[3] * plane_eq[0];
	sm[1][3] = -lightpos[3] * plane_eq[1];
	sm[2][3] = -lightpos[3] * plane_eq[2];
	sm[3][3] = dot - lightpos[3] * plane_eq[3];
}


// findplane
// Calculate A,B,C,D version of plane equation,
// given 3 non-colinear points in plane
void findplane(GLdouble plane_eq[4],
	const GLdouble p0[3],
	const GLdouble p1[3],
	const GLdouble p2[3])
{
	GLdouble vec0[3], vec1[3];

	// Need 2 vectors to find cross product.
	vec0[0] = p1[0] - p0[0];
	vec0[1] = p1[1] - p0[1];
	vec0[2] = p1[2] - p0[0];

	vec1[0] = p2[0] - p0[0];
	vec1[1] = p2[1] - p0[1];
	vec1[2] = p2[2] - p0[2];

	// find cross product to get A, B, and C of plane equation
	plane_eq[0] = vec0[1] * vec1[2] - vec0[2] * vec1[1];
	plane_eq[1] = vec0[2] * vec1[0] - vec0[0] * vec1[2];
	plane_eq[2] = vec0[0] * vec1[1] - vec0[1] * vec1[0];

	GLdouble normlen = sqrt(plane_eq[0] * plane_eq[0]
		+ plane_eq[1] * plane_eq[1]
		+ plane_eq[2] * plane_eq[2]);
	if (normlen != 0)
	{
		plane_eq[0] /= normlen;
		plane_eq[1] /= normlen;
		plane_eq[2] /= normlen;
	}
	else
	{
		plane_eq[0] = 1.;
		plane_eq[1] = 0.;
		plane_eq[2] = 0.;
	}

	plane_eq[3] = -(plane_eq[0] * p0[0]
		+ plane_eq[1] * p0[1]
		+ plane_eq[2] * p0[2]);
}

Model_OBJ::Model_OBJ()
{
	this->TotalConnectedTriangles = 0;
	this->TotalConnectedPoints = 0;
}

float* Model_OBJ::calculateNormal(float *coord1, float *coord2, float *coord3)
{
	/* calculate Vector1 and Vector2 */
	float va[3], vb[3], vr[3], val;
	va[0] = coord1[0] - coord2[0];
	va[1] = coord1[1] - coord2[1];
	va[2] = coord1[2] - coord2[2];

	vb[0] = coord1[0] - coord3[0];
	vb[1] = coord1[1] - coord3[1];
	vb[2] = coord1[2] - coord3[2];

	/* cross product */
	vr[0] = va[1] * vb[2] - vb[1] * va[2];
	vr[1] = vb[0] * va[2] - va[0] * vb[2];
	vr[2] = va[0] * vb[1] - vb[0] * va[1];

	/* normalization factor */
	val = sqrt(vr[0] * vr[0] + vr[1] * vr[1] + vr[2] * vr[2]);

	float norm[3];
	norm[0] = vr[0] / val;
	norm[1] = vr[1] / val;
	norm[2] = vr[2] / val;


	return norm;
}


int Model_OBJ::Load(char* filename)
{
	string line;
	ifstream objFile(filename);
	if (objFile.is_open())													// If obj file is open, continue
	{
		objFile.seekg(0, ios::end);										// Go to end of the file, 
		long fileSize = objFile.tellg();									// get file size
		objFile.seekg(0, ios::beg);										// we'll use this to register memory for our 3d model

		vertexBuffer = (float*)malloc(fileSize);							// Allocate memory for the verteces
		Faces_Triangles = (float*)malloc(fileSize*sizeof(float));			// Allocate memory for the triangles
		normals = (float*)malloc(fileSize*sizeof(float));					// Allocate memory for the normals

		int triangle_index = 0;												// Set triangle index to zero
		int normal_index = 0;												// Set normal index to zero

		while (!objFile.eof())											// Start reading file data
		{
			getline(objFile, line);											// Get line from file

			if (line.c_str()[0] == 'v')										// The first character is a v: on this line is a vertex stored.
			{
				line[0] = ' ';												// Set first character to 0. This will allow us to use sscanf

				sscanf(line.c_str(), "%f %f %f",							// Read floats from the line: v X Y Z
					&vertexBuffer[TotalConnectedPoints],
					&vertexBuffer[TotalConnectedPoints + 1],
					&vertexBuffer[TotalConnectedPoints + 2]);

				TotalConnectedPoints += POINTS_PER_VERTEX;					// Add 3 to the total connected points
			}


			if ((line[0] == 'f') && (line.find_first_of('/') == -1))		// The first character is an 'f': on this line is a point stored
			{
				line[0] = ' ';												// Set first character to 0. This will allow us to use sscanf

				int vertexNumber[4] = { 0, 0, 0 };

				sscanf(line.c_str(), "%d %d %d",						    // Read integers from the line:  f 1 2 3
					&vertexNumber[0],										// First point of our triangle. This is an 
					&vertexNumber[1],										// pointer to our vertexBuffer list
					&vertexNumber[2]);										// each point represents an X,Y,Z.

				vertexNumber[0] -= 1;										// OBJ file starts counting from 1
				vertexNumber[1] -= 1;										// OBJ file starts counting from 1
				vertexNumber[2] -= 1;										// OBJ file starts counting from 1


				/********************************************************************
				* Create triangles (f 1 2 3) from points: (v X Y Z) (v X Y Z) (v X Y Z).
				* The vertexBuffer contains all verteces
				* The triangles will be created using the verteces we read previously
				*/

				int tCounter = 0;
				for (int i = 0; i < POINTS_PER_VERTEX; i++)
				{
					Faces_Triangles[triangle_index + tCounter] = vertexBuffer[3 * vertexNumber[i]];
					Faces_Triangles[triangle_index + tCounter + 1] = vertexBuffer[3 * vertexNumber[i] + 1];
					Faces_Triangles[triangle_index + tCounter + 2] = vertexBuffer[3 * vertexNumber[i] + 2];
					tCounter += POINTS_PER_VERTEX;
				}

				/*********************************************************************
				* Calculate all normals, used for lighting
				*/
				float coord1[3] = { Faces_Triangles[triangle_index], Faces_Triangles[triangle_index + 1], Faces_Triangles[triangle_index + 2] };
				float coord2[3] = { Faces_Triangles[triangle_index + 3], Faces_Triangles[triangle_index + 4], Faces_Triangles[triangle_index + 5] };
				float coord3[3] = { Faces_Triangles[triangle_index + 6], Faces_Triangles[triangle_index + 7], Faces_Triangles[triangle_index + 8] };
				float *norm = this->calculateNormal(coord1, coord2, coord3);

				tCounter = 0;
				for (int i = 0; i < POINTS_PER_VERTEX; i++)
				{
					normals[normal_index + tCounter] = norm[0];
					normals[normal_index + tCounter + 1] = norm[1];
					normals[normal_index + tCounter + 2] = norm[2];
					tCounter += POINTS_PER_VERTEX;
				}

				triangle_index += TOTAL_FLOATS_IN_TRIANGLE;
				normal_index += TOTAL_FLOATS_IN_TRIANGLE;
				TotalConnectedTriangles += TOTAL_FLOATS_IN_TRIANGLE;
			}
			else if (line[0] == 'f') { // entrada do tipo n/n/n/ n/n/n/ n/n/n/

				line[0] = ' ';												// Set first character to 0. This will allow us to use sscanf

				int vertexNumber[4] = { 0, 0, 0 };

				line = line.substr((line.find_first_of(' ') + 1), line.length());
				line = line.substr((line.find_first_of(' ') + 1), line.length());

				string n1 = line.substr(0, (line.find_first_of('/')));

				vertexNumber[0] = atoi(n1.c_str());

				line = line.substr((line.find_first_of(' ') + 1), line.length());

				string n2 = line.substr(0, (line.find_first_of('/')));

				vertexNumber[1] = atoi(n2.c_str());

				line = line.substr((line.find_first_of(' ') + 1), line.length());

				string n3 = line.substr(0, (line.find_first_of('/')));

				vertexNumber[2] = atoi(n3.c_str());

				vertexNumber[0] -= 1;										// OBJ file starts counting from 1
				vertexNumber[1] -= 1;										// OBJ file starts counting from 1
				vertexNumber[2] -= 1;										// OBJ file starts counting from 1

				/********************************************************************
				* Create triangles (f 1 2 3) from points: (v X Y Z) (v X Y Z) (v X Y Z).
				* The vertexBuffer contains all verteces
				* The triangles will be created using the verteces we read previously
				*/

				int tCounter = 0;
				for (int i = 0; i < POINTS_PER_VERTEX; i++)
				{
					Faces_Triangles[triangle_index + tCounter] = vertexBuffer[3 * vertexNumber[i]];
					Faces_Triangles[triangle_index + tCounter + 1] = vertexBuffer[3 * vertexNumber[i] + 1];
					Faces_Triangles[triangle_index + tCounter + 2] = vertexBuffer[3 * vertexNumber[i] + 2];
					tCounter += POINTS_PER_VERTEX;
				}

				/*********************************************************************
				* Calculate all normals, used for lighting
				*/
				float coord1[3] = { Faces_Triangles[triangle_index], Faces_Triangles[triangle_index + 1], Faces_Triangles[triangle_index + 2] };
				float coord2[3] = { Faces_Triangles[triangle_index + 3], Faces_Triangles[triangle_index + 4], Faces_Triangles[triangle_index + 5] };
				float coord3[3] = { Faces_Triangles[triangle_index + 6], Faces_Triangles[triangle_index + 7], Faces_Triangles[triangle_index + 8] };
				float *norm = this->calculateNormal(coord1, coord2, coord3);

				tCounter = 0;
				for (int i = 0; i < POINTS_PER_VERTEX; i++)
				{
					normals[normal_index + tCounter] = norm[0];
					normals[normal_index + tCounter + 1] = norm[1];
					normals[normal_index + tCounter + 2] = norm[2];
					tCounter += POINTS_PER_VERTEX;
				}

				triangle_index += TOTAL_FLOATS_IN_TRIANGLE;
				normal_index += TOTAL_FLOATS_IN_TRIANGLE;
				TotalConnectedTriangles += TOTAL_FLOATS_IN_TRIANGLE;

				if (line.find_first_of(' ') != -1){

					line = line.substr((line.find_first_of(' ') + 1), line.length());

					string n4 = line.substr(0, (line.find_first_of('/')));

					vertexNumber[1] = atoi(n4.c_str());

					vertexNumber[1] -= 1;										// OBJ file starts counting from 1

					int tCounter = 0;

					for (int i = 0; i < POINTS_PER_VERTEX; i++)
					{
						Faces_Triangles[triangle_index + tCounter] = vertexBuffer[3 * vertexNumber[i]];
						Faces_Triangles[triangle_index + tCounter + 1] = vertexBuffer[3 * vertexNumber[i] + 1];
						Faces_Triangles[triangle_index + tCounter + 2] = vertexBuffer[3 * vertexNumber[i] + 2];
						tCounter += POINTS_PER_VERTEX;
					}

					/*********************************************************************
					* Calculate all normals, used for lighting
					*/
					float coord1[3] = { Faces_Triangles[triangle_index], Faces_Triangles[triangle_index + 1], Faces_Triangles[triangle_index + 2] };
					float coord2[3] = { Faces_Triangles[triangle_index + 3], Faces_Triangles[triangle_index + 4], Faces_Triangles[triangle_index + 5] };
					float coord3[3] = { Faces_Triangles[triangle_index + 6], Faces_Triangles[triangle_index + 7], Faces_Triangles[triangle_index + 8] };
					float *norm = this->calculateNormal(coord1, coord2, coord3);

					tCounter = 0;
					for (int i = 0; i < POINTS_PER_VERTEX; i++)
					{
						normals[normal_index + tCounter] = norm[0];
						normals[normal_index + tCounter + 1] = norm[1];
						normals[normal_index + tCounter + 2] = norm[2];
						tCounter += POINTS_PER_VERTEX;
					}

					triangle_index += TOTAL_FLOATS_IN_TRIANGLE;
					normal_index += TOTAL_FLOATS_IN_TRIANGLE;
					TotalConnectedTriangles += TOTAL_FLOATS_IN_TRIANGLE;

				}
			}
		}
		objFile.close();														// Close OBJ file
	}
	else
	{
		cout << "Unable to open file";
	}
	return 0;
}

void Model_OBJ::Release()
{
	free(this->Faces_Triangles);
	free(this->normals);
	free(this->vertexBuffer);
}

void Model_OBJ::Draw()
{
	glEnableClientState(GL_VERTEX_ARRAY);						// Enable vertex arrays
	glEnableClientState(GL_NORMAL_ARRAY);						// Enable normal arrays
	glVertexPointer(3, GL_FLOAT, 0, Faces_Triangles);			// Vertex Pointer to triangle array
	glNormalPointer(GL_FLOAT, 0, normals);						// Normal pointer to normal array
	glDrawArrays(GL_TRIANGLES, 0, TotalConnectedTriangles);		// Draw the triangles
	glDisableClientState(GL_VERTEX_ARRAY);						// Disable vertex arrays
	glDisableClientState(GL_NORMAL_ARRAY);						// Disable normal arrays
}

/***************************************************************************
* Program code
***************************************************************************/

Model_OBJ obj0, obj1, obj2;
float g_rot_x, g_rot_y, g_rot_z, g_rotation_x, g_rotation_y, g_rotation_z, g_translation_x, g_translation_y, g_translation_z = -20, g_scale = 1, posicao_luz_x = -5, posicao_luz_y, posicao_luz_z;

GLfloat cameraXpos = 0, cameraYpos = 0, cameraZpos = 0;
GLfloat cameraXang = 0, cameraYang = 270;
GLfloat red = 1.0, green = 1.0, blue = 1.0;
bool p = 0; // 0 = pinta uma face, 1 = pinta todo o objeto

GLfloat objectXpos = 0, objectYpos = 0, objectZpos = -200;
GLfloat objectXang = 0, objectYang = 0, objectZang = 0;

GLfloat cameraMatrix[16] = { 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0, 0, 0, 0, 0, 1.0 };

double Screenw = 400, Screenh = 400;

glutWindow win;

void translateMatrix(GLfloat m[16], GLfloat tX, GLfloat tY, GLfloat tZ){
	m[0] = m[0] + m[12] * tX;
	m[1] = m[1] + m[13] * tX;
	m[2] = m[2] + m[14] * tX;
	m[3] = m[3] + m[15] * tX;

	m[4] = m[4] + m[12] * tY;
	m[5] = m[5] + m[13] * tY;
	m[6] = m[6] + m[14] * tY;
	m[7] = m[7] + m[15] * tY;

	m[8] = m[8] + m[12] * tZ;
	m[9] = m[9] + m[13] * tZ;
	m[10] = m[10] + m[14] * tZ;
	m[11] = m[11] + m[15] * tZ;


}

void xRotateMatrix(GLfloat m[16], GLfloat angle){
	GLfloat cosAngle = cos(angle*PI / 180.0);
	GLfloat sinAngle = sin(angle*PI / 180.0);

	m[4] = cosAngle*m[4] - sinAngle*m[8];
	m[5] = cosAngle*m[5] - sinAngle*m[9];
	m[6] = cosAngle*m[6] - sinAngle*m[10];
	m[7] = cosAngle*m[7] - sinAngle*m[11];
	m[8] = sinAngle*m[4] + cosAngle*m[8];
	m[9] = sinAngle*m[5] + cosAngle*m[9];
	m[10] = sinAngle*m[6] + cosAngle*m[10];
	m[11] = sinAngle*m[7] + cosAngle*m[11];
}

void yRotateMatrix(GLfloat m[16], GLfloat angle){
	GLfloat cosAngle = cos(angle*PI / 180.0);
	GLfloat sinAngle = sin(angle*PI / 180.0);

	m[0] = cosAngle*m[0] + sinAngle*m[8];
	m[1] = cosAngle*m[1] + sinAngle*m[9];
	m[2] = cosAngle*m[2] + sinAngle*m[10];
	m[3] = cosAngle*m[3] + sinAngle*m[11];
	m[8] = -sinAngle*m[0] + cosAngle*m[8];
	m[9] = -sinAngle*m[1] + cosAngle*m[9];
	m[10] = -sinAngle*m[2] + cosAngle*m[10];
	m[11] = -sinAngle*m[3] + cosAngle*m[11];
}




void display()
{

	glColor3f(red, green, blue);
	GLfloat luzAmbiente0[4] = { 1.0, 1.0, 1.0, 1.0 }; // luz branca
	GLfloat luzAmbiente1[4] = { 1.0, 0.0, 0.0, 1.0 }; // luz vermelha
	GLfloat luzAmbiente2[4] = { 1.0, 0.0, 1.0, 1.0 }; // luz rosa
	GLfloat luzAmbiente3[4] = { 0.0, 0.0, 1.0, 1.0 }; // luz azul
	GLfloat luzAmbiente4[4] = { 0.0, 1.0, 0.0, 1.0 }; // luz verde
	GLfloat luzAmbiente5[4] = { 0.0, 1.0, 1.0, 1.0 }; // luz ciano
	GLfloat luzAmbiente6[4] = { 1.0, 0.3, 0.0, 1.0 }; // luz laranja
	GLfloat luzAmbiente7[4] = { 0.6, 0.0, 1.0, 1.0 }; // luz roxo

	GLfloat luzDifusa[4] = { 0.7, 0.7, 0.7, 1.0 };
	GLfloat luzEspecular[4] = { 1.0, 1.0, 1.0, 1.0 };

	GLfloat posicaoLuz0[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz1[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz2[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz3[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz4[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz5[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz6[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };
	GLfloat posicaoLuz7[4] = { posicao_luz_x, posicao_luz_y, posicao_luz_z, 1.0 };

	glEnable(GL_LIGHTING);

	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente0);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente1);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente2);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente3);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente4);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente5);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente6);
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, luzAmbiente7);

	glLightfv(GL_LIGHT0, GL_AMBIENT, luzAmbiente0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT0, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz0);

	glLightfv(GL_LIGHT1, GL_AMBIENT, luzAmbiente1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT1, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT1, GL_POSITION, posicaoLuz1);

	glLightfv(GL_LIGHT2, GL_AMBIENT, luzAmbiente2);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT2, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT2, GL_POSITION, posicaoLuz2);

	glLightfv(GL_LIGHT3, GL_AMBIENT, luzAmbiente3);
	glLightfv(GL_LIGHT3, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT3, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT3, GL_POSITION, posicaoLuz3);

	glLightfv(GL_LIGHT4, GL_AMBIENT, luzAmbiente4);
	glLightfv(GL_LIGHT4, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT4, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT4, GL_POSITION, posicaoLuz4);

	glLightfv(GL_LIGHT5, GL_AMBIENT, luzAmbiente5);
	glLightfv(GL_LIGHT5, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT5, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT5, GL_POSITION, posicaoLuz5);

	glLightfv(GL_LIGHT6, GL_AMBIENT, luzAmbiente6);
	glLightfv(GL_LIGHT6, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT6, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT6, GL_POSITION, posicaoLuz6);

	glLightfv(GL_LIGHT7, GL_AMBIENT, luzAmbiente7);
	glLightfv(GL_LIGHT7, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT7, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT7, GL_POSITION, posicaoLuz7);

	glLoadIdentity();

	glViewport(win.width / 2, 0, win.width / 2, win.height);
	glScissor(win.width / 2, 0, win.width / 2, win.height);
	glEnable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glPushMatrix();

	glRotatef(g_rot_x, 1, 0, 0);
	glRotatef(g_rot_y, 0, 1, 0);
	glRotatef(g_rot_z, 0, 0, 1);
	glTranslatef(g_translation_x, g_translation_y, g_translation_z);
	glScalef(g_scale, g_scale, g_scale);

	// Ativa o uso da luz ambiente 
	if (luz == 0){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente0);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glEnable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 1){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente1);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 2){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente2);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 3){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente3);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glEnable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 4){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente4);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glEnable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 5){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente5);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glEnable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 6){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente6);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glEnable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 7){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente7);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glEnable(GL_LIGHT7);
	}
	else {
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
		glDisable(GL_LIGHTING);
	}

	GLfloat diff[] = { red, green, blue };
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diff);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
	if (objeto == 0){
		obj0.Draw();
	}
	else if (objeto == 1) {
		obj1.Draw();
	}
	else {
		obj2.Draw();
	}

	
	glPushMatrix();
	glColor3f(1.0, 0.0, 0.0);
	glTranslatef(5.0, -2.0, 20.0);
	glutSolidCube(5);
	glTranslatef(-10.0, -2.0, -50.0);
	glColor3f(0.0, 1.0, 0.0);
	glutSolidCube(5);
	glPopMatrix();
	
	glPopMatrix();

	glPushMatrix();
	//operacoes de camera
	glTranslatef(cameraXpos, cameraYpos, cameraZpos);

	glRotatef(-cameraXang, 1.0f, 0, 0);
	glRotatef(-cameraYang, 0, 1.0f, 0);

	glPushMatrix();
	//draw camera
	//glTranslated(0, 0, 30);

	glRotatef(270, 0, 1.0f, 0);

	glColor3f(1.0f, 0.0f, 1.0f);
	glutSolidCube(2.0f);

	glTranslated(0, 0, -1);
	glColor3f(0.0f, 0.0f, 1.0f);
	glutSolidCube(1.0f);
	glTranslated(0, 0, 1);

	glPopMatrix();

	glPopMatrix();

	// ------------------------- Início desenho paleta de cores -------------------------

	// Elemento (1,1) da matriz de cores

	glViewport(win.width - 100.0, 0, 25.0, 25.0);
	glScissor(win.width - 100.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(1.0f, 1.0f, 1.0f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (1,2) da matriz de cores

	glViewport(win.width - 75.0, 0, 25.0, 25.0);
	glScissor(win.width - 75.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.68f, 0.68f, 0.68f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (1,3) da matriz de cores

	glViewport(win.width - 50.0, 0, 25.0, 25.0);
	glScissor(win.width - 50.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.45f, 0.45f, 0.45f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (1,4) da matriz de cores

	glViewport(win.width - 25.0, 0, 25.0, 25.0);
	glScissor(win.width - 25.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.05f, 0.05f, 0.05f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (2,1) da matriz de cores

	glViewport(win.width - 100.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 100.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.78f, 0.88f, 0.71f, 0.5f); // alterar aqui para mudar cor quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (2,2) da matriz de cores

	glViewport(win.width - 75.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 75.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.66f, 0.81f, 0.56f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (2,3) da matriz de cores

	glViewport(win.width - 50.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 50.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.1f, 0.68f, 0.32f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (2,4) da matriz de cores

	glViewport(win.width - 25.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 25.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.33f, 0.5f, 0.22f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,1) da matriz de cores

	glViewport(win.width - 100.0, 50.0, 25.0, 25.0);
	glScissor(win.width - 100.0, 50.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.74f, 0.84f, 0.93f, 0.5f); // alterar aqui para mudar cor quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,2) da matriz de cores

	glViewport(win.width - 75.0, 50.0, 25.0, 25.0);
	glScissor(win.width - 75.0, 50.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.56f, 0.67f, 0.85f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,3) da matriz de cores

	glViewport(win.width - 50.0, 50.0, 25.0, 25.0);
	glScissor(win.width - 50.0, 50.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.19f, 0.46f, 0.7f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,4) da matriz de cores

	glViewport(win.width - 25.0, 50.0, 25.0, 25.0);
	glScissor(win.width - 25.0, 50.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.19f, 0.34f, 0.58f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (4,1) da matriz de cores

	glViewport(win.width - 100.0, 75.0, 25.0, 25.0);
	glScissor(win.width - 100.0, 75.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(1.0f, 1.0f, 0.21f, 0.0f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (4,2) da matriz de cores

	glViewport(win.width - 75.0, 75.0, 25.0, 25.0);
	glScissor(win.width - 75.0, 75.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(1.0f, 0.75f, 0.17f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (4,3) da matriz de cores

	glViewport(win.width - 50.0, 75.0, 25.0, 25.0);
	glScissor(win.width - 50.0, 75.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(1.0f, 0.05f, 0.1f, 0.5f); // alterar aqui para mudar a cor do quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (4,4) da matriz de cores

	glViewport(win.width - 25.0, 75.0, 25.0, 25.0);
	glScissor(win.width - 25.0, 75.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.74f, 0.02f, 0.06f, 0.5f); // alterar aqui para mudar cor quadrado

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);


	//------------------------------------------------------------------------------------

	glViewport(0, 0, win.width / 2, win.height);
	glScissor(0, 0, win.width / 2, win.height);
	glEnable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	gluPerspective(angle, fAspect, 0.4, 1000);

	glMatrixMode(GL_MODELVIEW);

	glColor3f(red, green, blue);

	glPushMatrix();

	GLfloat temp[16];

	temp[0] = cameraMatrix[0];
	temp[1] = cameraMatrix[4];
	temp[2] = cameraMatrix[8];
	temp[3] = cameraMatrix[12];
	temp[4] = cameraMatrix[1];
	temp[5] = cameraMatrix[5];
	temp[6] = cameraMatrix[9];
	temp[7] = cameraMatrix[13];
	temp[8] = cameraMatrix[2];
	temp[9] = cameraMatrix[6];
	temp[10] = cameraMatrix[10];
	temp[11] = cameraMatrix[14];
	temp[12] = cameraMatrix[3];
	temp[13] = cameraMatrix[7];
	temp[14] = cameraMatrix[11];
	temp[15] = cameraMatrix[15];

	glLoadIdentity();
	glLoadMatrixf(temp);

	glRotatef(g_rot_x, 1, 0, 0);
	glRotatef(g_rot_y, 0, 1, 0);
	glRotatef(g_rot_z, 0, 0, 1);
	glTranslatef(g_translation_x, g_translation_y, g_translation_z);
	glScalef(g_scale, g_scale, g_scale);

	// Ativa o uso da luz ambiente 
	if (luz == 0){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente0);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glEnable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 1){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente1);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 2){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente2);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 3){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente3);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glEnable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 4){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente4);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glEnable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 5){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente5);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glEnable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 6){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente6);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glEnable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
	}
	else if (luz == 7){
		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, luzAmbiente7);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, luzDifusa);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, luzEspecular);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glEnable(GL_LIGHT7);
	}
	else {
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
		glDisable(GL_LIGHT2);
		glDisable(GL_LIGHT3);
		glDisable(GL_LIGHT4);
		glDisable(GL_LIGHT5);
		glDisable(GL_LIGHT6);
		glDisable(GL_LIGHT7);
		glDisable(GL_LIGHTING);
	}


	if (objeto == 0){
		obj0.Draw();
	}
	else if (objeto == 1) {
		obj1.Draw();
	}
	else {
		obj2.Draw();
	}

	
	glPushMatrix();
	glColor3f(1.0, 0.0, 0.0);
	glTranslatef(5.0, -2.0, 20.0);
	glutSolidCube(5);
	glTranslatef(-10.0, -2.0, -50.0);
	glColor3f(0.0, 1.0, 0.0);
	glutSolidCube(5);
	glPopMatrix();
	

	glPopMatrix();

	glutSwapBuffers();

}


// Função usada para especificar o volume de visualização
void EspecificaParametrosVisualizacao(void)
{
	// Especifica sistema de coordenadas de projeção
	glMatrixMode(GL_PROJECTION);

	// Inicializa sistema de coordenadas de projeção
	glLoadIdentity();

	// Especifica a projeção perspectiva
	gluPerspective(angle, fAspect, 0.4, 1000);

	// Especifica sistema de coordenadas do modelo
	glMatrixMode(GL_MODELVIEW);

	// Inicializa sistema de coordenadas do modelo
	glLoadIdentity();

}

void reshape(int w, int h)
{

	// Para previnir uma divisão por zero
	if (h == 0) h = 1;

	win.height = h;
	win.width = w;

	// Calcula a correção de aspecto
	fAspect = (GLfloat)w / (GLfloat)h;

	// Especifica o tamanho da viewport

	EspecificaParametrosVisualizacao();

}

void initialize()
{
	glMatrixMode(GL_PROJECTION);
	GLfloat aspect = (GLfloat)win.width / win.height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(win.field_of_view_angle, aspect, win.z_near, win.z_far);
	glMatrixMode(GL_MODELVIEW);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_NORMALIZE);
	glClearColor(0.0f, 0.1f, 0.0f, 0.5f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	// Capacidade de brilho do material
	GLfloat especularidade[4] = { 1.0, 1.0, 1.0, 1.0 };
	GLint especMaterial = 60;

	// Especifica que a cor de fundo da janela será preta
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

	// Habilita o modelo de colorização de Gouraud
	glShadeModel(GL_SMOOTH);

	// Define a refletância do material 
	glMaterialfv(GL_FRONT, GL_SPECULAR, especularidade);
	// Define a concentração do brilho
	glMateriali(GL_FRONT, GL_SHININESS, especMaterial);

	// Habilita a definição da cor do material a partir da cor corrente
	glEnable(GL_COLOR_MATERIAL);
	//Habilita o uso de iluminação
	glEnable(GL_LIGHTING);

	// Habilita o depth-buffering
	glEnable(GL_DEPTH_TEST);



	angle = 45;
}


void handleKeypress(unsigned char key, int x, int y)
{
	GLfloat cs, cx, cy, cz;

	switch (key){

		// interacao com o obj
		// P.S.: Assumindo que sempre existe um modelo ou fonte de luz selecionada

		// seleciona o modelo anterior (ao chegar no primeiro modelo, passa para a última fonte de luz)
	case 44: // tecla , ou 
	case 60: // tecla <

		if (objeto > 0){
			objeto--;
		}
		else{
			objeto = 2;
			if (luz > -1){
				luz--;
			}
			else{
				luz = 7;
			}
		}


		break;

		// seleciona o modelo seguinte (ao chegar no último modelo, passa para a primeira fonte de luz)
	case 46: // tecla . ou 
	case 62: // tecla >

		if (objeto < 2){
			objeto++;
		}
		else{
			objeto = 0;
			if (luz < 7){
				luz++;
			}
			else{
				luz = -1;
			}
		}

		break;

		// translada o objeto selecionado no eixo X no sentido negativo em coordenadas de mundo
	case 49: // tecla 1
		if (luz == -1){
			g_translation_x--;
		}
		else {
			posicao_luz_x--;
		}
		break;

		// translada o objeto selecionado no eixo X no sentido positivo em coordenadas de mundo
	case 50: // tecla 2
		if (luz == -1){
			g_translation_x++;
		}
		else {
			posicao_luz_x++;
		}
		break;

		// translada o objeto selecionado no eixo Y no sentido negativo em coordenadas de mundo
	case 51: // tecla 3
		if (luz == -1){
			g_translation_y--;
		}
		else {
			posicao_luz_y--;
		}
		break;

		// translada o objeto selecionado no eixo Y no sentido positivo em coordenadas de mundo
	case 52: // tecla 4
		if (luz == -1){
			g_translation_y++;
		}
		else {
			posicao_luz_y++;
		}
		break;

		// translada o objeto selecionado no eixo Z no sentido negativo em coordenadas de mundo
	case 53: // tecla 5
		if (luz == -1){
			g_translation_z--;
		}
		else {
			posicao_luz_z--;
		}
		break;

		// translada o objeto selecionado no eixo Z no sentido positivo em coordenadas de mundo
	case 54: // tecla 6
		if (luz == -1){
			g_translation_z++;
		}
		else {
			posicao_luz_z++;
		}
		break;

		// gira o objeto selecionado em relação ao eixo X
	case 55: // tecla 7
		if (luz == -1){
			g_rot_x++;
		}
		//g_rotation_x = 1;
		break;

		// gira o objeto selecionado em relação ao eixo Y
	case 56: // tecla 8 
		if (luz == -1){
			g_rot_y++;
		}
		//g_rotation_y = 1;
		break;

		// gira o objeto selecionado em relação ao eixo Z
	case 57: // tecla 9 
		if (luz == -1){
			g_rot_z++;
		}
		//g_rotation_z = 1;
		break;

		// modifica a escala do objeto decrementando seu tamanho em 1%
	case 45: // tecla - ou 
	case 95: // tecla _
		if (luz == -1){
			g_scale = g_scale * 0.9;
		}
		break;

		// modifica a escala do objeto incrementando seu tamanho em 1%
	case 43: // tecla + ou
	case 61: // tecla =
		if (luz == -1){
			g_scale = g_scale * 1.1;
		}
		break;

		// interacao com a camera

		// move para frente de acordo com o vetor diretor (eixo Z em coordenadas de câmera)
	case 119: // tecla w ou
	case 87: // tecla W
		cs = 0.5 * cos(cameraXang*PI / 180.0);
		cx = cs * cos(cameraYang*PI / 180.0);
		cy = 0.5 * sin(cameraXang*PI / 180.0);
		cz = cs * sin(cameraYang*PI / 180.0);

		cameraXpos += cx;
		cameraYpos += cy;
		cameraZpos += cz;

		translateMatrix(cameraMatrix, 0, 0, 0.5);
		break;

		// move para trás de acordo com o vetor diretor (eixo Z em coordenadas de câmera)
	case 115: // tecla s ou
	case 83: // tecla S
		cs = 0.5 * cos(cameraXang*PI / 180.0);
		cx = cs * cos(cameraYang*PI / 180.0);
		cy = 0.5 * sin(cameraXang*PI / 180.0);
		cz = cs * sin(cameraYang*PI / 180.0);

		cameraXpos -= cx;
		cameraYpos -= cy;
		cameraZpos -= cz;

		translateMatrix(cameraMatrix, 0, 0, -0.5);
		break;

		// move para a esquerda de acordo com o vetor lateral (eixo X em coordenadas de câmera)
	case 97: // tecla a ou
	case 65: // tecla A
		cameraXpos -= 0.3;
		translateMatrix(cameraMatrix, 0.3, 0, 0);
		break;

		// move para a direita de acordo com o vetor lateral (eixo X em coordenadas de câmera)
	case 100: // tecla d ou
	case 68: // tecla D
		cameraXpos += 0.3;
		translateMatrix(cameraMatrix, -0.3, 0, 0);
		break;

	case 70: // p
		p = !p;
		break;

	case 27: // ESC
		exit(0);
		break;

	}

	glutPostRedisplay();

}

int dragging = 0;

GLfloat currentX = 0;
GLfloat currentY = 0;

void handleMouseMove(int x, int y){
	if (dragging){

		cameraYang += (x - currentX);
		cameraXang += (y - currentY);

		yRotateMatrix(cameraMatrix, (x - currentX));
		xRotateMatrix(cameraMatrix, (y - currentY));

		currentX = x;
		currentY = y;

		glutPostRedisplay();
	}
}

void handleMouseClick(int button, int state, int x, int y)
{

	if (button == GLUT_LEFT_BUTTON)
	if (state == GLUT_DOWN){
		dragging = 1;

		currentX = x;
		currentY = y;
	}

	if (button == GLUT_LEFT_BUTTON)
	if (state == GLUT_UP){

		if ((x > win.width - 100) && (x < win.width - 75)){
			if ((y < win.height) && (y > win.height - 25)){ // (1,1)
				red = 1.0;
				green = 1.0;
				blue = 1.0;
			}

			if ((y < win.height - 25) && (y > win.height - 50)){ // (2,1)

				red = 0.78;
				green = 0.88;
				blue = 0.71;
			}

			if ((y < win.height - 50) && (y > win.height - 75)){ // ((3,1)
				red = 0.74;
				green = 0.84;
				blue = 0.93;
			}

			if ((y < win.height - 75) && (y > win.height - 100)){ // (4,1)
				red = 1.00;
				green = 1.0;
				blue = 0.21;
			}

		}

		if ((x > win.width - 75) && (x < win.width - 50)){

			if ((y < win.height) && (y > win.height - 25)){ // (1,2)
				red = 0.68;
				green = 0.68;
				blue = 0.68;
			}

			if ((y < win.height - 25) && (y > win.height - 50)){ // (2,2)
				red = 0.66;
				green = 0.81;
				blue = 0.56;
			}

			if ((y < win.height - 50) && (y > win.height - 75)){ // ((3,2)
				red = 0.56;
				green = 0.67;
				blue = 0.85;
			}

			if ((y < win.height - 75) && (y > win.height - 100)){ // (4,2)
				red = 1.00;
				green = 0.75;
				blue = 0.17;
			}

		}


		if ((x > win.width - 50) && (x < win.width - 25)){

			if ((y < win.height) && (y > win.height - 25)){ // (1,3)
				red = 0.45;
				green = 0.45;
				blue = 0.45;
			}

			if ((y < win.height - 25) && (y > win.height - 50)){ // (2,3)
				red = 0.1;
				green = 0.68;
				blue = 0.32;
			}

			if ((y < win.height - 50) && (y > win.height - 75)){ // ((3,3)
				red = 0.19;
				green = 0.46;
				blue = 0.7;
			}

			if ((y < win.height - 75) && (y > win.height - 100)){ // (4,3)
				red = 1.00;
				green = 0.05;
				blue = 0.1;
			}

		}


		if ((x > win.width - 25) && (x < win.width)){

			if ((y < win.height) && (y > win.height - 25)){ // (1,4)
				red = 0.05;
				green = 0.05;
				blue = 0.05;
			}

			if ((y < win.height - 25) && (y > win.height - 50)){ // (2,4)
				red = 0.33;
				green = 0.5;
				blue = 0.22;
			}

			if ((y < win.height - 50) && (y > win.height - 75)){ // ((3,4)
				red = 0.19;
				green = 0.34;
				blue = 0.58;
			}

			if ((y < win.height - 75) && (y > win.height - 100)){ // (4,4)
				red = 0.74;
				green = 0.02;
				blue = 0.06;
			}

		}

		dragging = 0;

	}

	glutPostRedisplay();  // avisa que a janela atual deve ser reimpressa

}

struct CVector3
{
public:

	// A default constructor
	CVector3() {}

	// This is our constructor that allows us to initialize our data upon creating an instance
	CVector3(float X, float Y, float Z)
	{
		x = X; y = Y; z = Z;
	}

	// Here we overload the + operator so we can add vectors together 
	CVector3 operator+(CVector3 vVector)
	{
		// Return the added vectors result.
		return CVector3(vVector.x + x, vVector.y + y, vVector.z + z);
	}

	// Here we overload the - operator so we can subtract vectors 
	CVector3 operator-(CVector3 vVector)
	{
		// Return the subtracted vectors result
		return CVector3(x - vVector.x, y - vVector.y, z - vVector.z);
	}

	// Here we overload the * operator so we can multiply by scalars
	CVector3 operator*(float num)
	{
		// Return the scaled vector
		return CVector3(x * num, y * num, z * num);
	}

	// Here we overload the / operator so we can divide by a scalar
	CVector3 operator/(float num)
	{
		// Return the scale vector
		return CVector3(x / num, y / num, z / num);
	}

	float x, y, z;
};

CVector3 GetOGLPos(int x, int y)
{
	GLint viewport[4];
	GLdouble modelview[16];
	GLdouble projection[16];
	GLfloat winX, winY, winZ;
	GLdouble posX, posY, posZ;

	glGetDoublev(GL_MODELVIEW_MATRIX, modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, projection);
	glGetIntegerv(GL_VIEWPORT, viewport);

	winX = (float)x;
	winY = (float)viewport[3] - (float)y;
	glReadPixels(x, int(winY), 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &winZ);

	gluUnProject(winX, winY, winZ, modelview, projection, viewport, &posX, &posY, &posZ);

	return CVector3(posX, posY, posZ);
}

void redesenha(){
	glutPostRedisplay();
}

int main(int argc, char **argv)
{
	// set window values
	win.width = 640;
	win.height = 480;
	win.title = "Projeto PG";
	win.field_of_view_angle = 45;
	win.z_near = 1.0f;
	win.z_far = 500.0f;

	// initialize and run program
	glutInit(&argc, argv);                                      // GLUT initialization
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_DEPTH);   // Display Mode
	glutInitWindowSize(win.width, win.height);					// set window size
	glutCreateWindow(win.title);								// create Window
	glutDisplayFunc(display);									// register Display Function
	glutIdleFunc(display);									    // register Idle Function
	glutKeyboardFunc(handleKeypress);						    // register Keyboard Handler

	glutMouseFunc(handleMouseClick);

	glutMotionFunc(handleMouseMove);

	glutReshapeFunc(reshape);

	glutIdleFunc(redesenha);

	initialize();

	// objs: apple (precisa dar um zoom out), cat, cow, cube, lion, shark, venus, whale, yoda (precisa dar um zoom out), cubo1, camel, chimp, ealge, spheretri
	obj0.Load("chimp.obj");
	obj1.Load("dog.obj");
	obj2.Load("shark.obj");

	CVector3 cor_clique = GetOGLPos(win.width - 25.0, 0);

	glutMainLoop();												// run GLUT mainloop

	return 0;
}