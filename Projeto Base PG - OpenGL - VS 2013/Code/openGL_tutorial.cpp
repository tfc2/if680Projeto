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

using namespace std;

GLfloat angle, fAspect;
int luz = -1, objeto = 0;


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


void drawplane()
{
	glColor3d(0.2, 0.2, 0.8);
	glBegin(GL_POLYGON);
	glVertex3dv(planepts[0]);
	glVertex3dv(planepts[1]);
	glVertex3dv(planepts[2]);
	glVertex3dv(planepts[3]);
	glEnd();
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
float g_rot_x, g_rot_y, g_rot_z, g_rotation_x, g_rotation_y, g_rotation_z, g_translation_x, g_translation_y, g_translation_z, g_scale = 1, posicao_luz_x = -5, posicao_luz_y, posicao_luz_z;

GLfloat cameraXpos = 0, cameraYpos = 0, cameraZpos = 0;
GLfloat cameraXrot = 0, cameraYrot = 0;

glutWindow win;

void display()
{

	glColor3f(0.8, 0.8, 0.8);
	GLfloat luzAmbiente0[4] = { 1.0, 1.0, 1.0, 1.0 }; // luz branca
	GLfloat luzAmbiente1[4] = { 1.0, 0.0, 0.0, 1.0 }; // luz vermelha
	GLfloat luzAmbiente2[4] = { 1.0, 0.0, 1.0, 1.0 }; // luz rosa
	GLfloat luzAmbiente3[4] = { 0.0, 0.0, 1.0, 1.0 }; // luz azul
	GLfloat luzAmbiente4[4] = { 0.0, 1.0, 0.0, 1.0 }; // luz verde
	GLfloat luzAmbiente5[4] = { 0.0, 1.0, 1.0, 1.0 }; // luz ciano
	GLfloat luzAmbiente6[4] = { 1.0, 0.3, 0.0, 1.0 }; // luz laranja
	GLfloat luzAmbiente7[4] = { 0.6, 0.0, 1.0, 1.0 }; // luz roxo

	GLfloat luzDifusa[4] = { 0.7, 0.7, 0.7, 1.0 };
	GLfloat luzEspecular[4] = { 0.4, 0.4, 0.4, 1.0 };

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

	// Define os parâmetros da luz de número 0 - branca
	glLightfv(GL_LIGHT0, GL_AMBIENT, luzAmbiente0);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT0, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT0, GL_POSITION, posicaoLuz0);

	// Define os parâmetros da luz de número 1 - vermelha
	glLightfv(GL_LIGHT1, GL_AMBIENT, luzAmbiente1);
	glLightfv(GL_LIGHT1, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT1, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT1, GL_POSITION, posicaoLuz1);

	// Define os parâmetros da luz de número 0 - branca
	glLightfv(GL_LIGHT2, GL_AMBIENT, luzAmbiente2);
	glLightfv(GL_LIGHT2, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT2, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT2, GL_POSITION, posicaoLuz2);

	// Define os parâmetros da luz de número 1 - vermelha
	glLightfv(GL_LIGHT3, GL_AMBIENT, luzAmbiente3);
	glLightfv(GL_LIGHT3, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT3, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT3, GL_POSITION, posicaoLuz3);

	// Define os parâmetros da luz de número 0 - branca
	glLightfv(GL_LIGHT4, GL_AMBIENT, luzAmbiente4);
	glLightfv(GL_LIGHT4, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT4, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT4, GL_POSITION, posicaoLuz4);

	// Define os parâmetros da luz de número 1 - vermelha
	glLightfv(GL_LIGHT5, GL_AMBIENT, luzAmbiente5);
	glLightfv(GL_LIGHT5, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT5, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT5, GL_POSITION, posicaoLuz5);

	// Define os parâmetros da luz de número 0 - branca
	glLightfv(GL_LIGHT6, GL_AMBIENT, luzAmbiente6);
	glLightfv(GL_LIGHT6, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT6, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT6, GL_POSITION, posicaoLuz6);

	// Define os parâmetros da luz de número 1 - vermelha
	glLightfv(GL_LIGHT7, GL_AMBIENT, luzAmbiente7);
	glLightfv(GL_LIGHT7, GL_DIFFUSE, luzDifusa);
	glLightfv(GL_LIGHT7, GL_SPECULAR, luzEspecular);
	glLightfv(GL_LIGHT7, GL_POSITION, posicaoLuz7);

	glLoadIdentity();
	gluLookAt(0, 20, 30, 0, 0, 0, 0, 1, 0);
	
	glViewport(0, 0, win.width, win.height);
	glScissor(0, 0, win.width, win.height);
	glEnable(GL_SCISSOR_TEST);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


	glPushMatrix();

	//operacoes de camera
	glRotatef(cameraYrot, 0, 1.0f, 0);
	glRotatef(cameraXrot, 1.0f, 0, 0);

	glTranslatef(-cameraXpos, -cameraYpos, -cameraZpos);

	glRotatef(g_rot_x, 1, 0, 0);
	glRotatef(g_rot_y, 0, 1, 0);
	glRotatef(g_rot_z, 0, 0, 1);
	glTranslatef(g_translation_x, g_translation_y, g_translation_z);
	glScalef(g_scale, g_scale, g_scale);

	// Ativa o uso da luz ambiente 
	if (luz == 0){
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
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);
		glEnable(GL_LIGHT2);
		glEnable(GL_LIGHT3);
		glEnable(GL_LIGHT4);
		glEnable(GL_LIGHT5);
		glEnable(GL_LIGHT6);
		glEnable(GL_LIGHT7);
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

	glPopMatrix();

	glDisable(GL_LIGHTING);

	// Draw the plane
	glPushMatrix();
	glTranslated(0., 0., -4.);
	drawplane();
	glPopMatrix();


	// Elemento (3,3) da matriz de cores

	glViewport(win.width - 50.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 50.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.5f, 0.5f, 0.0f, 0.5f);

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,4) da matriz de cores

	glViewport(win.width - 25.0, 25.0, 25.0, 25.0);
	glScissor(win.width - 25.0, 25.0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.5f, 0.0f, 0.5f, 0.5f);

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (3,4) da matriz de cores

	glViewport(win.width - 50.0, 0, 25.0, 25.0);
	glScissor(win.width - 50.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.5f, 0.5f, 0.5f);

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	// Elemento (4,4) da matriz de cores

	glViewport(win.width - 25.0, 0, 25.0, 25.0);
	glScissor(win.width - 25.0, 0, 25.0, 25.0);
	glEnable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 1.1f, 0.0f, 0.5f);

	glClear(GL_COLOR_BUFFER_BIT);

	glClearColor(0.0f, 0.0f, 0.0f, 0.5f);

	glColor3f(1.0, 1.0, 0.0);

	glPopMatrix();

	glPopMatrix();
	


	glutSwapBuffers();

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
		cameraZpos -= 10;
		break;

		// move para trás de acordo com o vetor diretor (eixo Z em coordenadas de câmera)
	case 115: // tecla s ou
	case 83: // tecla S
		cameraZpos += 10;
		break;

		// move para a esquerda de acordo com o vetor lateral (eixo X em coordenadas de câmera)
	case 97: // tecla a ou
	case 65: // tecla A
		cameraXpos -= 10;
		break;

		// move para a direita de acordo com o vetor lateral (eixo X em coordenadas de câmera)
	case 100: // tecla d ou
	case 68: // tecla D
		cameraXpos += 10;
		break;

		/*
		Falta fazer: botão esquerdo do mouse pressionado seguido de movimento do mouse: usar o delta de movimento
		(diferença entre X e Y antigos e atuais do ponteiro) para rotacionar o vetor diretor da câmera para
		esquerda / direita de acordo com o delta X e cima / baixo de acordo com o delta Y.A rotação lateral deve
		considerar o eixo Y do mundo virtual, enquanto a rotação vertical deve considerar o eixo lateral X da
		câmera.Dica : para aplicar rotação em torno de um eixo arbitrário w usar a matriz de rotação de
		Rodrigues(http ://mathworld.wolfram.com/RodriguesRotationFormula.html):
		*/

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

		//cameraYrot += (x - currentX);
		//cameraXrot += (currentY - y);

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
		dragging = 0;
	}

	glutPostRedisplay();  // avisa que a janela atual deve ser reimpressa
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

	glutIdleFunc(redesenha);

	initialize();

	// objs: apple (precisa dar um zoom out), cat, cow, cube, lion, shark, venus, whale, yoda (precisa dar um zoom out)
	obj0.Load("cat.obj");
	obj1.Load("dog.obj");
	obj2.Load("lion.obj");

	glutMainLoop();												// run GLUT mainloop

	return 0;
}