///////////////////////////////////////////////////////////////////////////////
// ViewManager.cpp
// ===============
// manage the viewing of 3D objects within the viewport
//
//  AUTHOR: Brian Battersby - SNHU Instructor / Computer Science
//  Updated for milestone camera input and projection toggles
///////////////////////////////////////////////////////////////////////////////

#include "ViewManager.h"

#include <cmath> // sin, cos, atan2, asin

// GLM Math Header inclusions
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// declaration of the global variables and defines
namespace
{
	// Variables for window width and height
	const int WINDOW_WIDTH = 1000;
	const int WINDOW_HEIGHT = 800;
	const char* g_ViewName = "view";
	const char* g_ProjectionName = "projection";

	// camera object used for viewing and interacting with
	// the 3D scene
	Camera* g_pCamera = nullptr;

	// these variables are used for mouse movement processing
	float gLastX = WINDOW_WIDTH / 2.0f;
	float gLastY = WINDOW_HEIGHT / 2.0f;
	bool gFirstMouse = true;

	// time between current frame and last frame
	float gDeltaTime = 0.0f;
	float gLastFrame = 0.0f;

	// projection mode
	bool bOrthographicProjection = false;

	// movement speed tuning
	float gBaseMoveSpeed = 6.0f;   // units per second
	float gSpeedScale = 1.0f;      // adjusted by mouse wheel

	// mouse look tuning
	float gMouseSensitivity = 0.10f;
	float gYaw = -90.0f;           // facing toward -Z
	float gPitch = 0.0f;

	// one-tap key handling
	bool gToggleKeyDown_O = false;
	bool gToggleKeyDown_P = false;

	// Orthographic camera settings (aim directly at the 3D object).
	// These are tuned for your current mug location:
	// BuildMug(glm::vec3(-2.0f, 0.68f, -1.0f));
	glm::vec3 gOrthoTarget = glm::vec3(-2.0f, 0.95f, -1.0f);
	glm::vec3 gOrthoCamPos = glm::vec3(-2.0f, 0.95f, 8.0f);
}

/***********************************************************
 *  ViewManager()
 ***********************************************************/
ViewManager::ViewManager(ShaderManager* pShaderManager)
{
	// initialize the member variables
	m_pShaderManager = pShaderManager;
	m_pWindow = NULL;

	g_pCamera = new Camera();

	// default camera view parameters
	g_pCamera->Position = glm::vec3(0.0f, 5.0f, 12.0f);
	g_pCamera->Front = glm::vec3(0.0f, -0.5f, -2.0f);
	g_pCamera->Up = glm::vec3(0.0f, 1.0f, 0.0f);
	g_pCamera->Zoom = 80.0f;

	// Initialize yaw and pitch from the default front direction
	glm::vec3 front = glm::normalize(g_pCamera->Front);

	// Use std::atan2/std::asin from <cmath> for portability
	gYaw = glm::degrees(static_cast<float>(std::atan2(front.z, front.x))) - 90.0f;
	gPitch = glm::degrees(static_cast<float>(std::asin(front.y)));
}

/***********************************************************
 *  ~ViewManager()
 ***********************************************************/
ViewManager::~ViewManager()
{
	// free up allocated memory
	m_pShaderManager = NULL;
	m_pWindow = NULL;

	if (NULL != g_pCamera)
	{
		delete g_pCamera;
		g_pCamera = NULL;
	}
}

/***********************************************************
 *  CreateDisplayWindow()
 ***********************************************************/
GLFWwindow* ViewManager::CreateDisplayWindow(const char* windowTitle)
{
	GLFWwindow* window = nullptr;

	// try to create the displayed OpenGL window
	window = glfwCreateWindow(
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		windowTitle,
		NULL, NULL);

	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return NULL;
	}

	glfwMakeContextCurrent(window);

	// capture mouse for FPS style camera look
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// callbacks for mouse look and scroll speed
	glfwSetCursorPosCallback(window, &ViewManager::Mouse_Position_Callback);
	glfwSetScrollCallback(window, &ViewManager::Mouse_Scroll_Callback);

	// enable blending for supporting transparent rendering
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	m_pWindow = window;
	return(window);
}

/***********************************************************
 *  Mouse_Position_Callback()
 *
 *  Mouse movement controls camera orientation (look).
 ***********************************************************/
void ViewManager::Mouse_Position_Callback(GLFWwindow* window, double xMousePos, double yMousePos)
{
	(void)window;

	float x = static_cast<float>(xMousePos);
	float y = static_cast<float>(yMousePos);

	if (gFirstMouse)
	{
		gLastX = x;
		gLastY = y;
		gFirstMouse = false;
	}

	float xoffset = x - gLastX;
	float yoffset = gLastY - y;  // reversed
	gLastX = x;
	gLastY = y;

	xoffset *= gMouseSensitivity;
	yoffset *= gMouseSensitivity;

	// Only apply mouse look while in perspective mode.
	// Orthographic mode is intended as a fixed inspection view.
	if (!bOrthographicProjection)
	{
		gYaw += xoffset;
		gPitch += yoffset;

		// Clamp pitch to prevent flipping
		if (gPitch > 89.0f) gPitch = 89.0f;
		if (gPitch < -89.0f) gPitch = -89.0f;

		glm::vec3 front;
		front.x = static_cast<float>(std::cos(glm::radians(gYaw)) * std::cos(glm::radians(gPitch)));
		front.y = static_cast<float>(std::sin(glm::radians(gPitch)));
		front.z = static_cast<float>(std::sin(glm::radians(gYaw)) * std::cos(glm::radians(gPitch)));
		g_pCamera->Front = glm::normalize(front);
	}
}

/***********************************************************
 *  Mouse_Scroll_Callback()
 *
 *  Mouse scroll adjusts movement speed.
 ***********************************************************/
void ViewManager::Mouse_Scroll_Callback(GLFWwindow* window, double xOffset, double yOffset)
{
	(void)window;
	(void)xOffset;

	gSpeedScale += static_cast<float>(yOffset) * 0.10f;
	if (gSpeedScale < 0.10f) gSpeedScale = 0.10f;
	if (gSpeedScale > 5.00f) gSpeedScale = 5.00f;
}

/***********************************************************
 *  ProcessKeyboardEvents()
 ***********************************************************/
void ViewManager::ProcessKeyboardEvents()
{
	// close the window if the escape key has been pressed
	if (glfwGetKey(m_pWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(m_pWindow, true);
	}

	// One-tap toggle for orthographic
	if (glfwGetKey(m_pWindow, GLFW_KEY_O) == GLFW_PRESS)
	{
		if (!gToggleKeyDown_O)
		{
			bOrthographicProjection = true;
			gToggleKeyDown_O = true;
		}
	}
	else
	{
		gToggleKeyDown_O = false;
	}

	// One-tap toggle for perspective
	if (glfwGetKey(m_pWindow, GLFW_KEY_P) == GLFW_PRESS)
	{
		if (!gToggleKeyDown_P)
		{
			bOrthographicProjection = false;
			gToggleKeyDown_P = true;
		}
	}
	else
	{
		gToggleKeyDown_P = false;
	}

	// Camera movement only applies in perspective mode
	if (!bOrthographicProjection)
	{
		const float velocity = gBaseMoveSpeed * gSpeedScale * gDeltaTime;

		// Forward and backward
		if (glfwGetKey(m_pWindow, GLFW_KEY_W) == GLFW_PRESS)
			g_pCamera->Position += g_pCamera->Front * velocity;
		if (glfwGetKey(m_pWindow, GLFW_KEY_S) == GLFW_PRESS)
			g_pCamera->Position -= g_pCamera->Front * velocity;

		// Left and right (strafe)
		glm::vec3 right = glm::normalize(glm::cross(g_pCamera->Front, g_pCamera->Up));
		if (glfwGetKey(m_pWindow, GLFW_KEY_A) == GLFW_PRESS)
			g_pCamera->Position -= right * velocity;
		if (glfwGetKey(m_pWindow, GLFW_KEY_D) == GLFW_PRESS)
			g_pCamera->Position += right * velocity;

		// Up and down
		if (glfwGetKey(m_pWindow, GLFW_KEY_Q) == GLFW_PRESS)
			g_pCamera->Position += g_pCamera->Up * velocity;
		if (glfwGetKey(m_pWindow, GLFW_KEY_E) == GLFW_PRESS)
			g_pCamera->Position -= g_pCamera->Up * velocity;
	}
}

/***********************************************************
 *  PrepareSceneView()
 ***********************************************************/
void ViewManager::PrepareSceneView()
{
	glm::mat4 view;
	glm::mat4 projection;

	// per-frame timing
	float currentFrame = static_cast<float>(glfwGetTime());
	gDeltaTime = currentFrame - gLastFrame;
	gLastFrame = currentFrame;

	// process any keyboard events that may be waiting in the event queue
	ProcessKeyboardEvents();

	// Define view and projection matrices based on current mode
	if (bOrthographicProjection)
	{
		// Orthographic: fixed camera aimed at the 3D object
		view = glm::lookAt(gOrthoCamPos, gOrthoTarget, glm::vec3(0.0f, 1.0f, 0.0f));

		float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

		// Tight framing for object inspection.
		// This helps ensure the bottom plane is not visible in orthographic mode.
		float orthoSize = 0.85f;

		projection = glm::ortho(
			-orthoSize * aspect, orthoSize * aspect,
			-orthoSize, orthoSize,
			0.1f, 100.0f);
	}
	else
	{
		// Perspective: normal free-look camera
		view = g_pCamera->GetViewMatrix();

		projection = glm::perspective(
			glm::radians(g_pCamera->Zoom),
			(GLfloat)WINDOW_WIDTH / (GLfloat)WINDOW_HEIGHT,
			0.1f, 100.0f);
	}

	// if the shader manager object is valid
	if (NULL != m_pShaderManager)
	{
		// set the view matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ViewName, view);

		// set the projection matrix into the shader for proper rendering
		m_pShaderManager->setMat4Value(g_ProjectionName, projection);

		// set the view position of the camera into the shader for proper rendering
		if (bOrthographicProjection)
		{
			m_pShaderManager->setVec3Value("viewPosition", gOrthoCamPos);
		}
		else
		{
			m_pShaderManager->setVec3Value("viewPosition", g_pCamera->Position);
		}
	}
}
