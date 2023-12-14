#define _USE_MATH_DEFINES

#include "Bestiole.h"
#include "Milieu.h"
#include "Config.h"
#include "capteur/Yeux.h"
#include "capteur/Oreilles.h"
#include "comportement/ComportementGregaire.h"
#include "comportement/ComportementPeureuse.h"
#include "comportement/ComportementKamikaze.h"
#include "comportement/ComportementPrevoyante.h"

#include <cstdlib>
#include <cmath>
#include <iostream>



using namespace std;

const double      Bestiole::AFF_SIZE = 8.;
const double      Bestiole::MAX_VITESSE = 10.;
int               Bestiole::next = 0;


Bestiole::Bestiole(void)
{
	identite = ++next;

	cout << "const Bestiole (" << identite << ") par defaut" << endl;

	x = y = 0;
	cumulX = cumulY = 0.;
	orientation = static_cast<double>(rand()) / RAND_MAX * 2. * M_PI;  // [0, 2*pi]
	vitesse = static_cast<double>(rand()) / RAND_MAX * MAX_VITESSE;

	couleur = new T[3];

	age = 0;
	maxAge = rand() % Config::MAX_AGE + Config::MIN_AGE;
	lastCollisionAge = 0;
	isAlive = true;
	isPeureuse = false;
	lastAgirAge = 0;

	setCapteurs();
	accessoires = Accessoires();

	isMultiple = (rand() % 5 == 0);
	setComportement();
}


Bestiole::Bestiole(const Bestiole& b)
{
	identite = ++next;

	cout << "const Bestiole (" << identite << ") par copie" << endl;

	x = b.x;
	y = b.y;
	cumulX = cumulY = 0.;
	orientation = b.orientation;
	vitesse = b.vitesse;
	couleur = new T[3];
	memcpy(couleur, b.couleur, 3 * sizeof(T));

	age = b.age;
	maxAge = b.maxAge;
	lastCollisionAge = b.lastCollisionAge;
	isAlive = b.isAlive;
	isMultiple = b.isMultiple;
	isPeureuse = b.isPeureuse;
	lastAgirAge = b.lastAgirAge;

	for (shared_ptr<ICapteur> pCapteurB : b.capteurs) {
		shared_ptr<ICapteur>  pCapteur(pCapteurB->clone());
		capteurs.push_back(pCapteur);
	}

	accessoires = Accessoires(b.accessoires);

	shared_ptr<IComportement>  pComportement(b.comportement->clone());
	comportement = pComportement;
}


Bestiole::~Bestiole(void)
{

	delete[] couleur;
	
	cout << "dest Bestiole" << this << endl;
}


bool operator==(const Bestiole& b1, const Bestiole& b2)
{
	return (b1.identite == b2.identite);
}


void Bestiole::initCoords(int xLim, int yLim)
{
	x = rand() % xLim;
	y = rand() % yLim;
}


void Bestiole::setCapteurs() {
	if (static_cast<double>(rand()) / RAND_MAX < 0.5) {
		shared_ptr<ICapteur> pCapteur(new Yeux());
		capteurs.push_back(pCapteur);
	}

	if (static_cast<double>(rand()) / RAND_MAX < 0.5) {
		shared_ptr<ICapteur> pCapteur(new Oreilles());
		capteurs.push_back(pCapteur);
	}
}


void Bestiole::setComportement() {
	int i = rand() % 4 + 1;
	switch (i) {
	case 1:
		comportement = make_shared<ComportementGregaire>();
		break;
	case 2:
		comportement = make_shared<ComportementPeureuse>();
		break;
	case 3:
		comportement = make_shared<ComportementKamikaze>();
		break;
	case 4:
		comportement = make_shared<ComportementPrevoyante>();
		break;
	default:
		cout << "Error: setComportement()" << endl;
	}

	setCouleur();
}


// set color according to comportement
void Bestiole::setCouleur()
{
	int couleurIdx = comportement->getComportementIdx();

	switch (couleurIdx) {
	case 1:
		couleur[0] = 50;
		couleur[1] = 205;
		couleur[2] = 50;
		break;
	case 2:
		couleur[0] = 148;
		couleur[1] = 0;
		couleur[2] = 211;
		break;
	case 3:
		couleur[0] = 255;
		couleur[1] = 0;
		couleur[2] = 0;
		break;
	case 4:
		couleur[0] = 30;
		couleur[1] = 144;
		couleur[2] = 255;
		break;

	default:
		couleur[0] = 169;
		couleur[1] = 169;
		couleur[2] = 169;
	}
}


// normalize the orientation in the [0, 2pi) range
double Bestiole::normalizeOrientation(double orientationNew) const {
	while (orientationNew < 0) {
		orientationNew += 2 * M_PI;
	}

	while (orientationNew >= 2 * M_PI) {
		orientationNew -= 2 * M_PI;
	}

	return orientationNew;
}


// get comportement information for statistics
int Bestiole::getComportementInfo() const
{
	int comportementInfo = 0;
	if (isMultiple)
		comportementInfo = 5;
	else
		comportementInfo = comportement->getComportementIdx();

	return comportementInfo;
}


// get capteurs information for statistics
list<int> Bestiole::getCapteurInfo() const
{
	list<int> capteurInfo;

	if (capteurs.size() == 0) {
		capteurInfo.push_back(0);
	}
	else {
		for (shared_ptr<ICapteur> pCapteur : capteurs) {
			if (pCapteur->getAlpha() == 2 * M_PI)
				capteurInfo.push_back(1);  // Oreilles
			else
				capteurInfo.push_back(2);  // Yeux
		}
	}
	return capteurInfo;
}


// get accessoires information for statistics
string Bestiole::getAccessoiresInfo() const
{
	return accessoires.getAccessoiresInfo();
}


// get the angular distance between this bestiole and another bestiole
double Bestiole::getAngularDistance(const Bestiole& b2) const
{
	double angular = acos((b2.x - x) / getDistance(b2));  // acos [0, pi]
	if (orientation > M_PI) {
		angular = 2 * M_PI - angular;
	}
	return angular;
}


void Bestiole::action(Milieu& monMilieu)
{
	/*
	Each bestiole goes through the following steps:
		1 - Aging and check if this bestiole is old to die
		2 - Check for collisions with other bestioles and if the involved bestioles die in collisions
		3 - Clone with probability Config::CLONAGE_RATE
		4 - Move
		5 - Detect other bestioles and set next speed and orientation according to the comportement
	*/
	
	aging();
	if (isAlive == false) return;

	checkCollision(monMilieu.getListeBestioles());
	if (isAlive == false) return;

	cloner(monMilieu.getListeBestioles(), monMilieu.getWidth(), monMilieu.getHeight());
	bouge(monMilieu.getWidth(), monMilieu.getHeight());
	detecterEtAgir(monMilieu.getListeBestioles());
}


// Aging and check if this bestiole is old to die
void Bestiole::aging() {
	if (age >= maxAge) {
		cout << "aged death Bestiole(" << identite << ")" << endl;
		isAlive = false;
	}
	else
		++age;
}


// Check for collisions with other bestioles and if the involved bestioles die in collisions
void Bestiole::checkCollision(list<shared_ptr<Bestiole>>& listeBestioles) {
	for (shared_ptr<Bestiole>& b : listeBestioles) {
		// If b is not this, and is alive
		if (!(*this == *b) && b->isAlive) {
			// If en collision
			if (getDistance(*b) <= Config::COLLISION_RANGE  && age - lastCollisionAge > Config::INTERVAL) {
				cout << "collison entre Bestiole(" << identite << ") et Bestiole(" << b->identite << ")" << endl;
				lastCollisionAge = age;
				b->lastCollisionAge = b->age;

				double r0 = static_cast<double>(rand()) / RAND_MAX;
				double r1 = static_cast<double>(rand()) / RAND_MAX;
				double deathRate0 = Config::COLLISION_DEATH_RATE * accessoires.getResistanceCoeff();
				double deathRate1 = Config::COLLISION_DEATH_RATE * b->accessoires.getResistanceCoeff();
				// cout << r0 << " " << r1 << endl;

				// check this bestiole should die or rebondir
				if (r0 < deathRate0) {
					cout << "collison death Bestiole(" << identite << ")" << endl;
					isAlive = false;
				}
				else
					rebondir();

				// check the other bestiole should die or rebondir
				if (r1 < deathRate1) {
					cout << "collison death Bestiole(" << b->identite << ")" << endl;
					b->isAlive = false;
				}
				else
					b->rebondir();
			}
		}
	}
}


// reverse the orientation
void Bestiole::rebondir() {
	cout << "rebodir Bestiole(" << identite << ")" << endl;
	orientation = normalizeOrientation(orientation + M_PI);
}


// Clone with probability Config::CLONAGE_RATE
void Bestiole::cloner(list<shared_ptr<Bestiole>>& listeBestioles, int xLim, int yLim) {
	double r = static_cast<double>(rand()) / RAND_MAX;
	if (r < Config::CLONAGE_RATE && isAlive) {
		cout << "clonage Bestiole(" << identite << ")" << endl;
		shared_ptr<Bestiole> pBestiole(new Bestiole(*this));
		pBestiole->age = 0;
		pBestiole->initCoords(xLim, yLim);
		pBestiole->orientation = static_cast<double>(rand()) / RAND_MAX * 2. * M_PI;
		pBestiole->lastCollisionAge = 0;
		pBestiole->isPeureuse = false;
		pBestiole->lastAgirAge = 0;
		listeBestioles.push_back(pBestiole);
	}
}


void Bestiole::bouge(int xLim, int yLim)
{
	double vitesseWithCoeff = vitesse * accessoires.getVitesseCoeff();
	if (vitesseWithCoeff > MAX_VITESSE) vitesseWithCoeff = MAX_VITESSE;

	double         nx, ny;
	double         dx = cos(orientation) * vitesseWithCoeff;
	double         dy = -sin(orientation) * vitesseWithCoeff;
	int            cx, cy;

	cx = static_cast<int>(cumulX); cumulX -= cx;
	cy = static_cast<int>(cumulY); cumulY -= cy;

	nx = x + dx + cx;
	ny = y + dy + cy;

	// if x reached the bound
	if ((nx < 0) || (nx > xLim - 1)) {
		orientation = normalizeOrientation(M_PI - orientation);
		cumulX = 0.;
	}
	else {
		x = static_cast<int>(nx);
		cumulX += nx - x;
	}

	// if y reached the bound
	if ((ny < 0) || (ny > yLim - 1)) {
		orientation = normalizeOrientation(-orientation);
		cumulY = 0.;
	}
	else {
		y = static_cast<int>(ny);
		cumulY += ny - y;
	}
}


void Bestiole::detecterEtAgir(list<shared_ptr<Bestiole>>& listeBestioles) {
	// detect other bestioles with capteurs
	set<shared_ptr<Bestiole>> bestiolesDetected;
	for (shared_ptr<ICapteur> pCapteur : capteurs) {
		for (shared_ptr<Bestiole>& b : listeBestioles) {
			if (!(*this == *b) && b->isAlive) {
				bool isdetected = pCapteur->isDetected(*this, *b);
				if (isdetected) {
					//system("pause");
					bestiolesDetected.insert(b);
				}
			}
		}
	}

	// For multiple personality bestioles, reset its comportement with probability Config::COMPORTEMTNT_CHANGE_RATE
	bool isSetComportement = false;
	if (isMultiple == true) {
		double r = static_cast<double>(rand()) / RAND_MAX;
		if (r < Config::COMPORTEMTNT_CHANGE_RATE) {
			cout << "changed comportement Bestiole(" << identite << ")" << endl;
			setComportement();
			isSetComportement = true;
		}
	}

	// Bestiole in a scared state, regain to the original speed after Config::PEUREUSE_DURATION or after resetting the comportement
	if (isPeureuse == true && (age - lastAgirAge >= Config::PEUREUSE_DURATION || isSetComportement)) {
		cout << "end peureuse Bestiole(" << identite << ")" << endl;
		vitesse /= Config::FUITE_COEFF;
		isPeureuse = false;
	}

	// set new speed and orientation according to the comportement
	// In order to avoid getting stuck when repeatedly setting orientation, using Config::INTERVAL 
	if (bestiolesDetected.size() > 0 && age - lastAgirAge > Config::INTERVAL) {
		double orientationNew = normalizeOrientation(comportement->changeOrientation(*this, bestiolesDetected));
		double vitesseNew = comportement->changeVitesse(*this, bestiolesDetected);

		if (orientation != orientationNew || vitesse != vitesseNew) {
			orientation = orientationNew;
			vitesse = vitesseNew;
			lastAgirAge = age;
		}
	}
}


void Bestiole::draw(UImg& support)
{
	double         xt = x + cos(orientation) * AFF_SIZE / 2.1;
	double         yt = y - sin(orientation) * AFF_SIZE / 2.1;
	support.draw_ellipse(x, y, AFF_SIZE, AFF_SIZE / 5., -orientation / M_PI * 180., couleur);
	support.draw_circle(xt, yt, AFF_SIZE / 2., couleur);

	string s = to_string(identite);
	if (isMultiple) s += " (m)";
	s += " " + accessoires.getAccessoiresInfo();
	char const* identiteChar = s.c_str();
	const T white[] = { (T)255, (T)255, (T)255 };
	support.draw_text(x - 6, y + AFF_SIZE, identiteChar, 0, white);

	/* draw capteurs */
	for (shared_ptr<ICapteur> pCapteur : capteurs) {
		if (pCapteur->getAlpha() == 2 * M_PI) {
			// draw oreilles range
			support.draw_circle(x, y, pCapteur->getDelta(), couleur, 0.1);
		}
		else {
			// draw yeux range
			double x2 = x + cos(orientation + pCapteur->getAlpha() / 2) * pCapteur->getDelta();
			double y2 = y - sin(orientation + pCapteur->getAlpha() / 2) * pCapteur->getDelta();
			double x3 = x + cos(orientation - pCapteur->getAlpha() / 2) * pCapteur->getDelta();
			double y3 = y - sin(orientation - pCapteur->getAlpha() / 2) * pCapteur->getDelta();
			support.draw_line(x, y, x2, y2, couleur, 0.2);
			support.draw_line(x, y, x3, y3, couleur, 0.2);
		}
	}
}
