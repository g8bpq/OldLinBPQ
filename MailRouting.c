/*
Copyright 2001-2015 John Wiseman G8BPQ

This file is part of LinBPQ/BPQ32.

LinBPQ/BPQ32 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

LinBPQ/BPQ32 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with LinBPQ/BPQ32.  If not, see http://www.gnu.org/licenses
*/	

// Mail and Chat Server for BPQ32 Packet Switch
//
// Message Routing Module

// This code decides where to send a message.

// Private messages are routed by TO and AT if possible, if not they follow the Bull Rules

// Bulls are routed on HA where possible

// Bulls should not be distributed outside their designated area.

#include "BPQMail.h"

char WW[] = "WW";

char * MyElements[20] = {WW};		// My HA in element format

char MyRouteElements[100];

int MyElementCount;

BOOL ReaddressLocal;
BOOL ReaddressReceived;
BOOL WarnNoRoute = TRUE;
BOOL Localtime = FALSE;		// Use Local Time for Timebands and forward connect scripts

struct ALIAS * CheckForNTSAlias(struct MsgInfo * Msg, char * FirstDestElement);
struct UserInfo * FindAMPR();

struct Continent
{
	char FourCharCode[5];
	char TwoCharCode[3];
};

struct Country
{
	char Country[5];
	char Continent4[5];
	char Continent2[3];
};


struct Continent Continents[] =
{
   		"EURO",	"EU", // Europe
   		"MEDR",	"EU", // Mediterranean
   		"ASIA",	"AS", // The Orient
   		"INDI",	"AS", // Indian Ocean including the Indian subcontinent
   		"MDLE",	"EU", // Middle East
   		"SEAS",	"EU", // South-East Asia
   		"NOAM",	"NA", // North America (Canada, USA, Mexico)
   		"CEAM",	"NA", // Central America
   		"CARB",	"NA", // Caribbean
   		"SOAM",	"SA", // South America
   		"AUNZ",	"OC", // Australia/New Zealand
   		"EPAC",	"OC", // Eastern Pacific
   		"NPAC",	"OC", // Northern Pacific
   		"SPAC",	"OC", // Southern Pacific
   		"WPAC",	"OC", // Western Pacific
   		"NAFR",	"AF", // Northern Africa
   		"CAFR",	"AF", // Central Africa
   		"SAFR",	"AF", // Southern Africa
   		"ANTR",	"OC", // Antarctica 
};

struct Country Countries[] = 
{			
		"AFG", "****", "AS", 		// Afghanistan
		"ALA", "EURO", "EU", 		// Åland Islands
		"ALB", "EURO", "EU", 		// Albania
		"DZA", "NAFR", "AF", 		// Algeria
		"ASM", "****", "AS", 		// American Samoa
		"AND", "EURO", "EU", 		// Andorra
		"AGO", "CAFR", "AF", 		// Angola
		"AIA", "CARB", "NA", 		// Anguilla
		"ATG", "CARB", "NA", 		// Antigua and Barbuda
		"ARG", "SOAM", "SA", 		// Argentina
		"ARM", "****", "AS", 		// Armenia
		"ABW", "CARB", "NA", 		// Aruba
		"AUS", "AUNZ", "OC", 		// Australia
		"AUT", "EURO", "EU", 		// Austria
		"AZE", "****", "AS", 		// Azerbaijan
		"BHS", "CARB", "NA", 		// Bahamas
		"BHR", "MDLE", "AS", 		// Bahrain
		"BGD", "INDE", "AS", 		// Bangladesh
		"BRB", "CARB", "NA", 		// Barbados
		"BLR", "EURO", "EU", 		// Belarus
		"BEL", "EURO", "EU", 		// Belgium
		"BLZ", "CEAM", "NA", 		// Belize
		"BEN", "CAFR", "AF", 		// Benin
		"BMU", "CARB", "NA", 		// Bermuda
		"BTN", "ASIA", "AS", 		// Bhutan
		"BOL", "SOAM", "SA", 		// Bolivia (Plurinational State of)
		"BIH", "EURO", "EU", 		// Bosnia and Herzegovina
		"BWA", "****", "AF", 		// Botswana
		"BRA", "****", "SA", 		// Brazil
		"VGB", "CARB", "NA", 		// British Virgin Islands
		"BRN", "ASIA", "AS", 		// Brunei Darussalam
		"BGR", "EURO", "EU", 		// Bulgaria
		"BFA", "CAFR", "AF", 		// Burkina Faso
		"BDI", "CAFR", "AF", 		// Burundi
		"KHM", "****", "AS", 		// Cambodia
		"CMR", "CAFR", "AF", 		// Cameroon
		"CAN", "NOAM", "NA", 		// Canada
		"CPV", "NAFR", "AF", 		// Cape Verde
		"CYM", "CARB", "NA", 		// Cayman Islands
		"CAF", "CAFR", "AF", 		// Central African Republic
		"TCD", "CAFR", "AF", 		// Chad
		"CHL", "SOAM", "SA", 		// Chile
		"CHN", "****", "AS", 		// China
		"HKG", "****", "AS", 		// Hong Kong Special Administrative Region of China
		"MAC", "****", "AS", 		// Macao Special Administrative Region of China
		"COL", "****", "SA", 		// Colombia
		"COM", "SAFR", "AF", 		// Comoros
		"COG", "****", "AF", 		// Congo
		"COK", "SPAC", "OC", 		// Cook Islands
		"CRI", "CEAM", "NA", 		// Costa Rica
		"CIV", "CAFR", "AF", 		// Côte d'Ivoire
		"HRV", "EURO", "EU", 		// Croatia
		"CUB", "CARB", "NA", 		// Cuba
		"CYP", "EURO", "EU", 		// Cyprus
		"CZE", "EURO", "EU", 		// Czech Republic
		"PRK", "****", "AS", 		// Democratic People's Republic of Korea
		"COD", "****", "AF", 		// Democratic Republic of the Congo
		"DNK", "EURO", "EU", 		// Denmark
		"DJI", "NAFR", "AF", 		// Djibouti
		"DMA", "CARB", "NA", 		// Dominica
		"DOM", "CARB", "NA", 		// Dominican Republic
		"ECU", "SOAM", "SA", 		// Ecuador
		"EGY", "MDLE", "AF", 		// Egypt
		"SLV", "CEAM", "NA", 		// El Salvador
		"GNQ", "CAFR", "AF", 		// Equatorial Guinea
		"ERI", "****", "AF", 		// Eritrea
		"EST", "EURO", "EU", 		// Estonia
		"ETH", "****", "AF", 		// Ethiopia
		"FRO", "EURO", "EU", 		// Faeroe Islands
		"FLK", "****", "SA", 		// Falkland Islands (Malvinas)
		"FJI", "SPAC", "OC", 		// Fiji
		"FIN", "EURO", "EU", 		// Finland
		"FRA", "EURO", "EU", 		// France
		"GUF", "SOAM", "SA", 		// French Guiana
		"PYF", "SPAC", "OC", 		// French Polynesia
		"GAB", "CAFR", "AF", 		// Gabon
		"GMB", "CAFR", "AF", 		// Gambia
		"GEO", "ASIA", "AS", 		// Georgia
		"DEU", "EURO", "EU", 		// Germany
		"GHA", "CAFR", "AF", 		// Ghana
		"GIB", "EURO", "EU", 		// Gibraltar
		"GRC", "EURO", "EU", 		// Greece
		"GRL", "EURO", "EU", 		// Greenland
		"GRD", "CARB", "NA", 		// Grenada
		"GLP", "CARB", "NA", 		// Guadeloupe
		"GUM", "SPAC", "OC", 		// Guam
		"GTM", "CEAM", "NA", 		// Guatemala
		"GGY", "EURO", "EU", 		// Guernsey
		"GIN", "CAFR", "AF", 		// Guinea
		"GNB", "CAFR", "AF", 		// Guinea-Bissau
		"GUY", "SOAM", "SA", 		// Guyana
		"HTI", "CARB", "NA", 		// Haiti
		"VAT", "EURO", "EU", 		// Holy See
		"HND", "CEAM", "NA", 		// Honduras
		"HUN", "EURO", "EU", 		// Hungary
		"ISL", "EURO", "EU", 		// Iceland
		"IND", "INDI", "AS", 		// India
		"IDN", "****", "AS", 		// Indonesia
		"IRN", "MDLE", "AS", 		// Iran (Islamic Republic of)
		"IRQ", "MDLE", "AS", 		// Iraq
		"IRL", "EURO", "EU", 		// Ireland
		"IMN", "EURO", "EU", 		// Isle of Man
		"ISR", "MDLE", "AS", 		// Israel
		"ITA", "EURO", "EU", 		// Italy
		"JAM", "****", "NA", 		// Jamaica
		"JPN", "****", "AS", 		// Japan
		"JEY", "EURO", "EU", 		// Jersey
		"JOR", "MDLE", "AS", 		// Jordan
		"KAZ", "****", "AS", 		// Kazakhstan
		"KEN", "****", "AF", 		// Kenya
		"KIR", "EPAC", "OC", 		// Kiribati
		"KWT", "MDLE", "AS", 		// Kuwait
		"KGZ", "ASIA", "AS", 		// Kyrgyzstan
		"LAO", "ASIA", "AS", 		// Lao People's Democratic Republic
		"LVA", "EURO", "EU", 		// Latvia
		"LBN", "MDLE", "AS", 		// Lebanon
		"LSO", "SAFR", "AF", 		// Lesotho
		"LBR", "CAFR", "AF", 		// Liberia
		"LBY", "MDLE", "AS", 		// Libyan Arab Jamahiriya
		"LIE", "EURO", "EU", 		// Liechtenstein
		"LTU", "EURO", "EU", 		// Lithuania
		"LUX", "EURO", "EU", 		// Luxembourg
		"MDG", "SAFR", "AF", 		// Madagascar
		"MWI", "SAFR", "AF", 		// Malawi
		"MYS", "ASIA", "AS", 		// Malaysia
		"MDV", "INDI", "AS", 		// Maldives
		"MLI", "CAFR", "AF", 		// Mali
		"MLT", "EURO", "EU", 		// Malta
		"MHL", "WPAC", "OC", 		// Marshall Islands
		"MTQ", "CARB", "NA", 		// Martinique
		"MRT", "NAFR", "AF", 		// Mauritania
		"MUS", "SAFR", "AF", 		// Mauritius
		"MYT", "SAFR", "AF", 		// Mayotte
		"MEX", "****", "NA", 		// Mexico
		"FSM", "WPAC", "OC", 		// Micronesia (Federated States of)
		"MCO", "EURO", "EU", 		// Monaco
		"MNG", "****", "AS", 		// Mongolia
		"MNE", "EURO", "EU", 		// Montenegro
		"MSR", "CARB", "NA", 		// Montserrat
		"MAR", "NAFR", "AF", 		// Morocco
		"MOZ", "SAFR", "AF", 		// Mozambique
		"MMR", "ASIA", "AS", 		// Myanmar
		"NAM", "****", "AF", 		// Namibia
		"NRU", "WPAC", "OC", 		// Nauru
		"NPL", "****", "AS", 		// Nepal
		"NLD", "EURO", "EU", 		// Netherlands
		"ANT", "CARB", "NA", 		// Netherlands Antilles
		"NCL", "SPAC", "OC", 		// New Caledonia
		"NZL", "AUNZ", "OC", 		// New Zealand
		"NIC", "****", "SA", 		// Nicaragua
		"NER", "NAFR", "AF", 		// Niger
		"NGA", "****", "AF", 		// Nigeria
		"NIU", "SPAC", "OC", 		// Niue
		"NFK", "SPAC", "OC", 		// Norfolk Island
		"MNP", "WPAC", "OC", 		// Northern Mariana Islands
		"NOR", "EURO", "EU", 		// Norway
		"PSE", "MDLE", "AS", 		// Occupied Palestinian Territory
		"OMN", "MDLE", "AS", 		// Oman
		"PAK", "INDI", "AS", 		// Pakistan
		"PLW", "SPAC", "OC", 		// Palau
		"PAN", "CEAM", "SA", 		// Panama
		"PNG", "SPAC", "OC", 		// Papua New Guinea
		"PRY", "SOAM", "SA", 		// Paraguay
		"PER", "SOAM", "SA", 		// Peru
		"PHL", "ASIA", "AS", 		// Philippines
		"PCN", "SPAC", "OC", 		// Pitcairn
		"POL", "EURO", "EU", 		// Poland
		"PRT", "EURO", "EU", 		// Portugal
		"PRI", "CARB", "NA", 		// Puerto Rico
		"QAT", "MDLE", "AS", 		// Qatar
		"KOR", "ASIA", "AS", 		// Republic of Korea
		"MDA", "EURO", "EU", 		// Republic of Moldova
		"REU", "SAFR", "AF", 		// Réunion
		"ROU", "EURO", "EU", 		// Romania
		"RUS", "ASIA", "AS", 		// Russian Federation
		"RWA", "****", "AF", 		// Rwanda
		"BLM", "CARB", "NA", 		// Saint-Barthélemy
		"SHN", "SOAM", "SA", 		// Saint Helena
		"KNA", "CARB", "NA", 		// Saint Kitts and Nevis
		"LCA", "CARB", "NA", 		// Saint Lucia
		"MAF", "CARB", "NA", 		// Saint-Martin (French part)
		"SPM", "NOAM", "NA", 		// Saint Pierre and Miquelon
		"VCT", "CARB", "NA", 		// Saint Vincent and the Grenadines
		"WSM", "SPAC", "OC", 		// Samoa
		"SMR", "EURO", "EU", 		// San Marino
		"STP", "CAFR", "AF", 		// Sao Tome and Principe
		"SAU", "MDLE", "AS", 		// Saudi Arabia
		"SEN", "CAFR", "AF", 		// Senegal
		"SRB", "EURO", "EU", 		// Serbia
		"SYC", "SAFR", "AF", 		// Seychelles
		"SLE", "****", "AF", 		// Sierra Leone
		"SGP", "****", "AS", 		// Singapore
		"SVK", "EURO", "EU", 		// Slovakia
		"SVN", "EURO", "EU", 		// Slovenia
		"SLB", "SPAC", "OC", 		// Solomon Islands
		"SOM", "****", "AF", 		// Somalia
		"ZAF", "SAFR", "AF", 		// South Africa
		"ESP", "EURO", "EU", 		// Spain
		"LKA", "INDE", "AS", 		// Sri Lanka
		"SDN", "****", "AF", 		// Sudan
		"SUR", "SOAM", "SA", 		// Suriname
		"SJM", "EURO", "EU", 		// Svalbard and Jan Mayen Islands
		"SWZ", "****", "AF", 		// Swaziland
		"SWE", "EURO", "EU", 		// Sweden
		"CHE", "EURO", "EU", 		// Switzerland
		"SYR", "MDLE", "AS", 		// Syrian Arab Republic
		"TJK", "ASIA", "AS", 		// Tajikistan
		"THA", "****", "AS", 		// Thailand
		"MKD", "EURO", "EU", 		// The former Yugoslav Republic of Macedonia
		"TLS", "ASIA", "AS", 		// Timor-Leste
		"TGO", "CAFR", "AF", 		// Togo
		"TKL", "AUNZ", "OC", 		// Tokelau
		"TON", "SPAC", "OC", 		// Tonga
		"TTO", "CARB", "NA", 		// Trinidad and Tobago
		"TUN", "****", "AF", 		// Tunisia
		"TUR", "EURO", "EU", 		// Turkey
		"TKM", "****", "AS", 		// Turkmenistan
		"TCA", "CARB", "NA", 		// Turks and Caicos Islands
		"TUV", "SPAC", "OC", 		// Tuvalu
		"UGA", "****", "AF", 		// Uganda
		"UKR", "EURO", "EU", 		// Ukraine
		"ARE", "MDLE", "AS", 		// United Arab Emirates
		"GBR", "EURO", "EU", 		// United Kingdom of Great Britain and Northern Ireland
		"TZA", "****", "AF", 		// United Republic of Tanzania
		"USA", "NOAM", "NA", 		// United States of America
		"VIR", "CARB", "NA", 		// United States Virgin Islands
		"URY", "SOAM", "SA", 		// Uruguay
		"UZB", "ASIA", "AS", 		// Uzbekistan
		"VUT", "SPAC", "OC", 		// Vanuatu
		"VEN", "SOAM", "SA", 		// Venezuela (Bolivarian Republic of)
		"VNM", "****", "AS", 		// Viet Nam
		"WLF", "SPAC", "OC", 		// Wallis and Futuna Islands
		"ESH", "****", "AF", 		// Western Sahara
		"YEM", "****", "AF", 		// Yemen
		"ZMB", "SAFR", "AF", 		// Zambia
		"ZWE", "SAFR", "AF"		// Zimbabwe
};
char ** AliasText;
struct ALIAS ** Aliases;

struct ALIAS ** NTSAliases = NULL;

/*struct ALIAS Aliases[] =
{
	"AMSAT",	"WW",
	"USBBS",	"USA",
	"ALLUS",	"USA"};
*/

int NumberofContinents = sizeof(Continents)/sizeof(Continents[1]);
int NumberofCountries = sizeof(Countries)/sizeof(Countries[1]);

struct Continent * FindContinent(char * Name)
{
	int i;
	struct Continent * Cont;

	for(i=0; i< NumberofContinents; i++)
	{
		Cont = &Continents[i];
		
		if ((_stricmp(Name, Cont->FourCharCode) == 0) || (_stricmp(Name, Cont->TwoCharCode) == 0))
			return Cont;

	}

	return NULL;
}

struct Country * FindCountry(char * Name)
{
	int i;
	struct Country * Cont;

	for(i=0; i< NumberofCountries; i++)
	{
		Cont = &Countries[i];
		
		if (_stricmp(Name, Cont->Country) == 0)
			return Cont;
	}

	return NULL;
}

struct ALIAS * FindAlias(char * Name)
{
	struct ALIAS ** Alias;

	Alias =  Aliases;

	while(Alias[0])
	{
		if (_stricmp(Name, Alias[0]->Dest) == 0)
			return Alias[0];
	
		Alias++;
	}

	return NULL;
}

VOID SetupMyHA()
{
	int Elements = 1;					// Implied WW on front
	char * ptr2;
	struct Continent * Continent;

	strcpy(MyRouteElements, HRoute);
	
	// Split it up

	ptr2 = MyRouteElements + strlen(MyRouteElements) - 1;

	do 
	{
		while ((*ptr2 != '.') && (ptr2 > MyRouteElements))
		{
			ptr2 --;
		}

		if (ptr2 == MyRouteElements)
		{
			// End
	
			MyElements[Elements++] = _strdup(ptr2);
			break;
		}

		MyElements[Elements++] = _strdup(ptr2+1);
		*ptr2 = 0;

	} while(Elements < 20);		// Just in case!

	MyElements[Elements++] = _strdup(BBSName);

	MyElementCount = Elements;

	if (MyElements[1])
	{
		if (strlen(MyElements[1]) == 4)
		{
			// Convert to 2 char Continent;
			Continent = FindContinent(MyElements[1]);
			if (Continent)
			{
				free(MyElements[1]);
				MyElements[1] = _strdup(Continent->TwoCharCode);
			}
		}
	}
}

VOID SetupNTSAliases(char * FN)
{
	FILE *in;
	char Buffer[2048];
	char *buf = Buffer;
	int Count = 0;
	char seps[] = " ,:/\t";
	char * Dest, * Alias, * Context;

	NTSAliases = zalloc(sizeof(struct ALIAS));

	in = fopen(FN, "r");

	if (in)
	{
		while(fgets(buf, 128, in))
		{
			strlop(buf, '\n');
			strlop(buf, '\r');			// in case Windows Format

			Dest = _strdup(buf);		// strtok changes it

			Dest = strtok_s(Dest, seps, &Context);
			if (Dest == NULL)
				continue;

			Alias = strtok_s(NULL, seps, &Context);
			if (Alias == NULL)
				continue;

			if (strlen(Dest) < 3 && (strchr(Dest, '*') == 0))
				continue;

			NTSAliases = realloc(NTSAliases, (Count+2)* sizeof(struct ALIAS));
			NTSAliases[Count] = zalloc(sizeof(struct ALIAS));

			NTSAliases[Count]->Dest =  Dest; 
			NTSAliases[Count]->Alias = Alias;

			Count++;
		}

		NTSAliases[Count] = NULL;
		fclose(in);
	}
}

VOID SetupFwdAliases()
{
	char ** Text = AliasText;
	char * Dest, * Alias;
	int Count = 0;
	char seps[] = " ,:/";

	Aliases = zalloc(sizeof(struct ALIAS));

	if (Text)
	{
		while(Text[0])
		{
			Aliases = realloc(Aliases, (Count+2)* sizeof(struct ALIAS));
			Aliases[Count] = zalloc(sizeof(struct ALIAS));

			Dest = _strdup(Text[0]);		// strtok changes it

			Dest = strtok_s(Dest, seps, &Alias);

			Aliases[Count]->Dest =  Dest; 
			Aliases[Count]->Alias = Alias;

			Count++;
			Text++;
		}
		Aliases[Count] = NULL;
	}
}

VOID SetupHAElements(struct BBSForwardingInfo * ForwardingInfo)
{
	char * HText = ForwardingInfo->BBSHA;
	char * SaveHText, * ptr2;
	struct Continent * Continent;
	int Elements = 1;

	ForwardingInfo->BBSHAElements = zalloc(8);				// always NULL entry on end even if no values

	SaveHText = _strdup(HText);

	ptr2 = SaveHText + strlen(SaveHText) -1;

	ForwardingInfo->BBSHAElements[0] = _strdup(WW);

	do 
	{
		ForwardingInfo->BBSHAElements = realloc(ForwardingInfo->BBSHAElements, (Elements+2)*4);
		
		while ((*ptr2 != '.') && (ptr2 > SaveHText))
		{
			ptr2 --;
		}

		if (ptr2 == SaveHText)
		{
			// End
	
			ForwardingInfo->BBSHAElements[Elements++] = _strdup(ptr2);
			break;
		}

			ForwardingInfo->BBSHAElements[Elements++] = _strdup(ptr2+1);
			*ptr2 = 0;

	} while(TRUE);

	ForwardingInfo->BBSHAElements[Elements++] = NULL;

	if (ForwardingInfo->BBSHAElements[1])
	{
		if (strlen(ForwardingInfo->BBSHAElements[1]) == 4)
		{
			// Convert to 2 char Continent;
			Continent = FindContinent(ForwardingInfo->BBSHAElements[1]);
			if (Continent)
			{
				free(ForwardingInfo->BBSHAElements[1]);
				ForwardingInfo->BBSHAElements[1] = _strdup(Continent->TwoCharCode);
			}
		}
	}

	free(SaveHText);

}

VOID SetupHAddreses(struct BBSForwardingInfo * ForwardingInfo)
{
	int Count=0;
	char ** HText = ForwardingInfo->Haddresses;
	char * SaveHText, * ptr2;
//	char * TopElement;
	char * Num;
	struct Continent * Continent;

	ForwardingInfo->HADDRS = zalloc(4);				// always NULL entry on end even if no values
	ForwardingInfo->HADDROffet = zalloc(4);			// always NULL entry on end even if no values

	while(HText[0])
	{
		int Elements = 1;
		ForwardingInfo->HADDRS = realloc(ForwardingInfo->HADDRS, (Count+2)*4);
		ForwardingInfo->HADDROffet = realloc(ForwardingInfo->HADDROffet, (Count+2)*4);
	
		ForwardingInfo->HADDRS[Count] = zalloc(8);	// Always at lesat WWW and NULL

		SaveHText = _strdup(HText[0]);
		Num = strlop(SaveHText, ',');

		ptr2 = SaveHText + strlen(SaveHText) -1;

		ForwardingInfo->HADDRS[Count][0] = _strdup(WW);

		if (strcmp(HText[0], "WW") != 0)
		{

		do 
		{
			ForwardingInfo->HADDRS[Count] = realloc(ForwardingInfo->HADDRS[Count], (Elements+2)*4);
			
			while ((*ptr2 != '.') && (ptr2 > SaveHText))
			{
				ptr2 --;
			}

			if (ptr2 == SaveHText)
			{
				// End
	
				ForwardingInfo->HADDRS[Count][Elements++] = _strdup(ptr2);
				break;
			}

			ForwardingInfo->HADDRS[Count][Elements++] = _strdup(ptr2+1);
			*ptr2 = 0;

		} while(TRUE);
		}

		ForwardingInfo->HADDRS[Count][Elements++] = NULL;

		// If the route is not a complete HR (ie starting with a continent)
		// Add elemets from BBS HA to complete it.
		// (How do we know how many??
		// ?? Still ont sure about this ??
		// What i was trying to do was end up with a full HR, but knowing how many elements to use.
		// Far simpler to config it, but can users cope??
		//	Will config for testing. HA, N

/*
		TopElement=ForwardingInfo->HADDRS[Count][0];

		if (strcmp(TopElement, MyElements[0]) == 0)
			goto FullHR;

		if (FindContinent(TopElement))
			goto FullHR;
	
		// Need to add stuff from our HR

		Elements--;

		if (Elements < MyElementCount)
			break;

FullHR:
*/

		ForwardingInfo->HADDROffet[Count] = (Num)? atoi(Num): 0;

		if (ForwardingInfo->HADDRS[Count][1])
		{
			if (strlen(ForwardingInfo->HADDRS[Count][1]) == 4)
			{
				// Convert to 2 char Continent;
				Continent = FindContinent(ForwardingInfo->HADDRS[Count][1]);
				if (Continent)
				{
					free(ForwardingInfo->HADDRS[Count][1]);
					ForwardingInfo->HADDRS[Count][1] = _strdup(Continent->TwoCharCode);
				}
			}
		}
		free(SaveHText);
		HText++;
		Count++;
	}

	ForwardingInfo->HADDRS[Count] = NULL;

}

VOID SetupHAddresesP(struct BBSForwardingInfo * ForwardingInfo)
{
	int Count=0;
	char ** HText = ForwardingInfo->HaddressesP;
	char * SaveHText, * ptr2;
//	char * TopElement;
	char * Num;
	struct Continent * Continent;

	ForwardingInfo->HADDRSP = zalloc(4);				// always NULL entry on end even if no values

	while(HText[0])
	{
		int Elements = 1;
		ForwardingInfo->HADDRSP = realloc(ForwardingInfo->HADDRSP, (Count+2)*4);
	
		ForwardingInfo->HADDRSP[Count] = zalloc(8);	// Always at lesat WWW and NULL

		SaveHText = _strdup(HText[0]);
		Num = strlop(SaveHText, ',');

		ptr2 = SaveHText + strlen(SaveHText) -1;

		ForwardingInfo->HADDRSP[Count][0] = _strdup(WW);

		if (strcmp(HText[0], "WW") != 0)
		{

		do 
		{
			ForwardingInfo->HADDRSP[Count] = realloc(ForwardingInfo->HADDRSP[Count], (Elements+2)*4);
			
			while ((*ptr2 != '.') && (ptr2 > SaveHText))
			{
				ptr2 --;
			}

			if (ptr2 == SaveHText)
			{
				// End
	
				ForwardingInfo->HADDRSP[Count][Elements++] = _strdup(ptr2);
				break;
			}

			ForwardingInfo->HADDRSP[Count][Elements++] = _strdup(ptr2+1);
			*ptr2 = 0;

		} while(TRUE);
		}

		ForwardingInfo->HADDRSP[Count][Elements++] = NULL;

		if (ForwardingInfo->HADDRSP[Count][1])
		{
			if (strlen(ForwardingInfo->HADDRSP[Count][1]) == 4)
			{
				// Convert to 2 char Continent;
				Continent = FindContinent(ForwardingInfo->HADDRSP[Count][1]);
				if (Continent)
				{
					free(ForwardingInfo->HADDRSP[Count][1]);
					ForwardingInfo->HADDRSP[Count][1] = _strdup(Continent->TwoCharCode);
				}
			}
		}
		free(SaveHText);
		HText++;
		Count++;
	}

	ForwardingInfo->HADDRSP[Count] = NULL;
}

VOID CheckAndSend(struct MsgInfo * Msg, CIRCUIT * conn, struct UserInfo * bbs) 
{
	struct BBSForwardingInfo * ForwardingInfo = bbs->ForwardingInfo;
		
	if ((_stricmp(bbs->Call, BBSName) != 0) || ForwardToMe)			// Dont forward to ourself - already here!
	{
		if ((conn == NULL) || (!(conn->BBSFlags & BBS) || (_stricmp(conn->UserPointer->Call, bbs->Call) != 0))) // Dont send back
		{
			set_fwd_bit(Msg->fbbs, bbs->BBSNumber);
			ForwardingInfo->MsgCount++;
			if (ForwardingInfo->SendNew)
				ForwardingInfo->FwdTimer = ForwardingInfo->FwdInterval;

		}
	}
}

int MatchMessagetoBBSList(struct MsgInfo * Msg, CIRCUIT * conn)
{
	struct UserInfo * bbs;
	struct UserInfo * user;
	struct BBSForwardingInfo * ForwardingInfo;
	char ATBBS[41]="";
	char RouteElements[41];
	int Count = 0;
	char * HElements[20] = {NULL};
	int Elements = 0;
	char * ptr2;
	int MyElement = 0;
	BOOL Flood = FALSE;
	char FullRoute[100];
	struct Continent * Continent;
	struct Country * Country;
	struct ALIAS * Alias;
	struct UserInfo * RMS;

	if (Msg->status == 'K')
		return 1;				// No point,  but don't want no route warning

	strcpy(RouteElements, Msg->via);

	Logprintf(LOG_BBS, conn, '?', "Msg %d Routing Trace To %s Via %s", Msg->number, Msg->to, RouteElements);

	//	"NTS" Alias substitution is now done on P and T before any other processing

	//	No, Not a good idea to use same aliss file for NTS and other messages
	//	(What about OT 2*?)
	//	If we need to alias other types, add a new file

	if (Msg->type == 'T')
	{
		strlop(RouteElements, '.');

		Alias = CheckForNTSAlias(Msg, RouteElements);

		if (Alias)
		{
			// Replace the AT in the message with the Alias

			Logprintf(LOG_BBS, conn, '?', "Routing Trace @%s taken from Alias File", Alias->Alias);
			strcpy(Msg->via, Alias->Alias);
			SaveMessageDatabase();
		}

		strcpy(RouteElements, Msg->via);		// May have changed
	}

//	See if sending @ winlink.org

	if (_stricmp(Msg->to, "RMS") == 0)
	{
		// If a user of this bbs with Poll RMS set, leave it here - no point in sending to winlink
	
		// To = RMS could come from RMS:EMAIL Address. If so, we only check user if @winlink.org, or
		// we will hold g8bpq@g8bpq.org.uk

		char * Call;
		char * AT;

		Call = _strupr(_strdup(Msg->via));
		AT = strlop(Call, '@');

		if (AT)
		{
			if (_stricmp(AT, "WINLINK.ORG") == 0)
			{
				user = LookupCall(Call);

				if (user)
				{
					if (user->flags & F_POLLRMS)
					{
						Logprintf(LOG_BBS, conn, '?', "Message @ winlink.org, but local RMS user - leave here");
						strcpy(Msg->to, Call);
						strcpy(Msg->via, AT);

						free(Call);

						if (user->flags & F_BBS)	// User is also a BBS, so set FWD bit so he can get it
							set_fwd_bit(Msg->fbbs, user->BBSNumber);
	
						return 1;
					}
				}
			}
		}
		free(Call);

		// To = RMS, but not winlink.org, or not local user.

		RMS = FindRMS();

		if (RMS)
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace to RMS Matches BBS RMS");
			
			set_fwd_bit(Msg->fbbs, RMS->BBSNumber);
			RMS->ForwardingInfo->MsgCount++;
			if (RMS->ForwardingInfo->SendNew)
				RMS->ForwardingInfo->FwdTimer = RMS->ForwardingInfo->FwdInterval;

			return 1;
		}

		// To RMS, but don't have a BBS called RMS - dropthrough in case we are forwarding RMS to another BBS

	}

	if (_stricmp(RouteElements, "WINLINK.ORG") == 0)
	{
		user = LookupCall(Msg->to);

		if (user)
		{
			if (user->flags & F_POLLRMS)
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace @ winlink.org, but local RMS user - leave here");
		
				if (user->flags & F_BBS)	// User is also a BBS, so set FWD bit so he can get it
					set_fwd_bit(Msg->fbbs, user->BBSNumber);

				return 1;					// Route found
			}
		}
	
		RMS = FindRMS();

		if (RMS)
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace @ winlink.org Matches BBS RMS");
			
			set_fwd_bit(Msg->fbbs, RMS->BBSNumber);
			RMS->ForwardingInfo->MsgCount++;
			if (RMS->ForwardingInfo->SendNew)
				RMS->ForwardingInfo->FwdTimer = RMS->ForwardingInfo->FwdInterval;

			return 1;
		}
		Logprintf(LOG_BBS, conn, '?', "Routing Trace - @ winlink.org but no BBS RMS");
		return 0;
	}

	// See if AMPR.ORG Mail

	// If for our domain leave alone

	if (AMPRDomain[0] && SendAMPRDirect && _stricmp(Msg->via, AMPRDomain) != 0)
	{
		int toLen = strlen(Msg->via);

		if (_memicmp(&Msg->via[toLen - 8], "ampr.org", 8) == 0)
		{
			if (_stricmp(Msg->to, "AMPR") != 0)	// Already set up?
			{
		
				// Put full name in VIA and AMPR in TO

				char Full[80];

				sprintf(Full, "%s@%s", Msg->to, Msg->via);

				if (strlen(Full) > 40)
					Full[40] = 0;

				strcpy(Msg->via, Full);

				strcpy(Msg->to, "AMPR");
			}
		}

	
		if (_stricmp(Msg->to, "AMPR") == 0)
		{
			bbs = FindAMPR();

			if (bbs)
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace to ampr.org Matches BBS AMPR");
			
				set_fwd_bit(Msg->fbbs, bbs->BBSNumber);
				bbs->ForwardingInfo->MsgCount++;
				if (bbs->ForwardingInfo->SendNew)
					bbs->ForwardingInfo->FwdTimer = bbs->ForwardingInfo->FwdInterval;

				return 1;
			}

			// To AMPR, but don't have a BBS called AMPR - dropthrough in case we are forwarding AMPR to another BBS
		}
	}

	// See if a well-known alias

	Alias = FindAlias(RouteElements);
	
	if (Alias)
	{
		Logprintf(LOG_BBS, conn, '?', "Routing Trace Alias Substitution %s > %s",
			RouteElements, Alias->Alias); 

		strcpy(RouteElements, Alias->Alias);

		if (conn)
			if ((ReaddressReceived && (conn->BBSFlags & BBS)) || (ReaddressLocal && ((conn->BBSFlags & BBS) == 0)))
				strcpy(Msg->via, Alias->Alias);
	}

	// Make sure HA is complete (starting at WW)

	if (RouteElements[0] == 0)
		goto NOHA;

	ptr2 = RouteElements + strlen(RouteElements) - 1;

	while ((*ptr2 != '.') && (ptr2 > RouteElements))
	{
		ptr2 --;
	}

	if (ptr2 != RouteElements)
		*ptr2++;

	if ((strcmp(ptr2, "WW") == 0) || (strcmp(ptr2, "WWW") == 0))
	{
		strcpy(FullRoute, RouteElements);
		goto FULLHA;
	}

	if (FindContinent(ptr2))
	{
		// Just need to add WW

		sprintf_s(FullRoute, sizeof(FullRoute),"%s.WW", RouteElements);
		goto FULLHA;
	}

	Country = FindCountry(ptr2);
	
	if (Country)
	{
		// Just need to add Continent and WW

		sprintf_s(FullRoute, sizeof(FullRoute),"%s.%s.WW", RouteElements, Country->Continent2);
		goto FULLHA;
	}

	// Don't know

	// Assume a local dis list, and set Flood.

	strcpy(FullRoute, RouteElements);

	if (Msg->type == 'B')
		Flood = TRUE;

FULLHA:

	strcpy(ATBBS, FullRoute);

	strlop(ATBBS, '.');

	if (FullRoute)
	{
		// Split it up

		ptr2 = FullRoute + strlen(FullRoute) - 1;

		do 
		{
			while ((*ptr2 != '.') && (ptr2 > FullRoute))
			{
				ptr2 --;
			}

			if (ptr2 != FullRoute)
				*ptr2++ = 0;
	
			HElements[Elements++] = ptr2;


		} while(Elements < 20 && ptr2 != FullRoute);		// Just in case!
	}

	if (HElements[1])
	{
		if (strlen(HElements[1]) == 4)
		{
			// Convert to 2 char Continent;
			Continent = FindContinent(HElements[1]);
			if (Continent)
			{
//				free(MyElements[1]);
				HElements[1] = _strdup(Continent->TwoCharCode);
			}
		}
	}


	// if Bull, see if reached right area

	if (Msg->type == 'B')
	{
		int i = 0;
		
		// All elements of Helements must match Myelements

		while (MyElements[i] && HElements[i]) // Until one set runs out
		{
			if (strcmp(MyElements[i], HElements[i]) != 0)
				break;
			i++;
		}

		// if HElements[i+1] = NULL, have matched all

		if (HElements[i] == NULL)
			Flood = TRUE;
	}

NOHA:

	Logprintf(LOG_BBS, conn, '?', "Routing Trace Type %c %sTO %s VIA %s Route On %s %s %s %s %s",
		Msg->type, (Flood) ? "(Flood) ":"", Msg->to, Msg->via, HElements[0],
		HElements[1], HElements[2], HElements[3], HElements[4]);


	if (Msg->type == 'T')
	{
		int depth = 0;
		int bestmatch = -1;
		struct UserInfo * bestbbs = NULL;

		//	if NTS Traffic. Route on Wildcarded TO (best match).

		//  If no match, route on AT (Should be NTSxx)

		//  If no match, send to any BBS with routing to state XX and NTS flag set (no we dont!)

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			depth = CheckBBSToForNTS(Msg, ForwardingInfo);

			if (depth > -1)
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace NTS Matches TO BBS %s Length %d", bbs->Call, depth);
		
				if (depth > bestmatch)
				{
					bestmatch = depth;
					bestbbs = bbs;
				}
			}
		}
		if (bestbbs)
		{
			if (bestbbs->flags  & F_NTSMPS) 
				Logprintf(LOG_BBS, conn, '?', "Routing Trace NTS Best Match is %s, but NTS MPS Set so not queued", bestbbs->Call);
			else
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace NTS Best Match is %s", bestbbs->Call);
				CheckAndSend(Msg, conn, bestbbs);
			}
			return 1;
		}

		// Check AT 

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			if (CheckBBSAtList(Msg, ForwardingInfo, ATBBS))
			{
				if (bbs->flags  & F_NTSMPS) 
					Logprintf(LOG_BBS, conn, '?', "Routing Trace NTS %s Matches AT %s, but NTS MPS Set so not queued", ATBBS, bbs->Call);
				else
				{
					Logprintf(LOG_BBS, conn, '?', "Routing Trace NTS %s Matches AT %s", ATBBS, bbs->Call);
					CheckAndSend(Msg, conn, bbs);
				}
				return 1;
			}
		}
		goto CheckWildCardedAT;
	}


	if (Msg->type == 'P' || Flood == 0)
	{
		// P messages are only sent to one BBS, but check the TO and AT of all BBSs before routing on HA,
		// and choose the best match on HA

		struct UserInfo * bestbbs = NULL;
		int bestmatch = 0;
		int depth;

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			if (ForwardingInfo->PersonalOnly && (Msg->type != 'P') && (Msg->type != 'T'))
				continue;
			
			if (CheckBBSToList(Msg, bbs, ForwardingInfo))
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace TO %s Matches BBS %s", Msg->to, bbs->Call);

				CheckAndSend(Msg, conn, bbs);
				return 1;
			}
		}

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			if (ForwardingInfo->PersonalOnly && (Msg->type != 'P'))
				continue;

			// Check implied AT 

			if ((strcmp(ATBBS, bbs->Call) == 0))			// @BBS = BBS		
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace %s Matches implied AT %s", ATBBS, bbs->Call);

		
				CheckAndSend(Msg, conn, bbs);

				return 1;
			}

			// Check AT 

			if (CheckBBSAtList(Msg, ForwardingInfo, ATBBS))
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace %s Matches AT %s", ATBBS, bbs->Call);

				CheckAndSend(Msg, conn, bbs);

				return 1;
			}
		}

		// We should choose the BBS with most matching elements (ie match on #23.GBR better that GBR)

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			if (ForwardingInfo->PersonalOnly && (Msg->type != 'P'))
				continue;

			depth = CheckBBSHElements(Msg, bbs, ForwardingInfo, ATBBS, &HElements[0]);

			if (depth)
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace HR Matches BBS %s Depth %d", bbs->Call, depth);
		
				if (depth > bestmatch)
				{
					bestmatch = depth;
					bestbbs = bbs;
				}
			}
		}
		if (bestbbs)
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace HR Best Match is %s", bestbbs->Call);

			CheckAndSend(Msg, conn, bestbbs);
		
			return 1;
		}

		// Check for wildcarded AT address

//		if (ATBBS[0] == 0)
//			return FALSE;			// no AT

CheckWildCardedAT:

		depth = 0;
		bestmatch = -1;
		bestbbs = NULL;

		for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
		{		
			ForwardingInfo = bbs->ForwardingInfo;

			if (ForwardingInfo->PersonalOnly && (Msg->type != 'P'))
				continue;

			depth = CheckBBSATListWildCarded(Msg, ForwardingInfo, ATBBS);

			if (depth > -1)
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace Wildcarded AT Matches  %s Length %d", bbs->Call, depth);
		
				if (depth > bestmatch)
				{
					bestmatch = depth;
					bestbbs = bbs;
				}
			}
		}
		if (bestbbs)
		{
			if (Msg->type == 'T' && (bestbbs->flags  & F_NTSMPS)) 
				Logprintf(LOG_BBS, conn, '?', "Routing Trace Wildcarded AT Best Match is %s, but NTS Msg and MPS Set so not queued", bestbbs->Call);
			else
			{
				Logprintf(LOG_BBS, conn, '?', "Routing Trace Wildcarded AT Best Match is %s", bestbbs->Call);
				CheckAndSend(Msg, conn, bestbbs);
			}
			return 1;
		}

		Logprintf(LOG_BBS, conn, '?', "Routing Trace - No Match");
		return FALSE;	// No match
	}

	// Flood Bulls go to all matching BBSs in the flood area, so the order of checking doesn't matter

	// For now I will only route on AT (for non-hierarchical addresses) and HA

	// Ver 1.0.4.2 - Try including TO

	for (bbs = BBSChain; bbs; bbs = bbs->BBSNext)
	{		
		ForwardingInfo = bbs->ForwardingInfo;

		if (ForwardingInfo->PersonalOnly)
			continue;

		if (CheckBBSToList(Msg, bbs, ForwardingInfo))
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace TO %s Matches BBS %s", Msg->to, bbs->Call);

			if ((_stricmp(bbs->Call, BBSName) != 0) || ForwardToMe)	// Dont forward to ourself - already here!
			{
				set_fwd_bit(Msg->fbbs, bbs->BBSNumber);
				ForwardingInfo->MsgCount++;
			}
			Count++;
			continue;
		}

		if ((strcmp(ATBBS, bbs->Call) == 0) ||			// @BBS = BBS		
			CheckBBSAtList(Msg, ForwardingInfo, ATBBS))
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace AT %s Matches BBS %s", Msg->to, bbs->Call);
			CheckAndSend(Msg, conn, bbs);

			Count++;
			continue;
		}
		
		
		if (CheckBBSHElementsFlood(Msg, bbs, ForwardingInfo, Msg->via, &HElements[0]))
		{
			Logprintf(LOG_BBS, conn, '?', "Routing Trace HR %s %s %s %s %s Matches BBS %s",
				HElements[0], HElements[1], HElements[2], 
				HElements[3], HElements[4], bbs->Call);
	
			CheckAndSend(Msg, conn, bbs);

			Count++;
		}

	}

	if (Count == 0)		
		Logprintf(LOG_BBS, conn, '?', "Routing Trace - No Match");

	return Count;
}

BOOL CheckBBSToList(struct MsgInfo * Msg, struct UserInfo * bbs, struct	BBSForwardingInfo * ForwardingInfo)
{
	char ** Calls;

	// Check TO distributions

	if (ForwardingInfo->TOCalls)
	{
		Calls = ForwardingInfo->TOCalls;

		while(Calls[0])
		{
			if (strcmp(Calls[0], Msg->to) == 0)	
				return TRUE;

			Calls++;
		}
	}
	return FALSE;
}

BOOL CheckBBSAtList(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS)
{
	char ** Calls;

	// Check AT distributions

//	if (strcmp(ATBBS, bbs->Call) == 0)			// @BBS = BBS
//		return TRUE;

	if (ForwardingInfo->ATCalls)
	{
		Calls = ForwardingInfo->ATCalls;

		while(Calls[0])
		{
			if (strcmp(Calls[0], ATBBS) == 0)	
				return TRUE;

			Calls++;
		}
	}
	return FALSE;
}

int CheckBBSHElements(struct MsgInfo * Msg, struct UserInfo * bbs, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS, char ** HElements)
{
	// Used for Personal Messages, and Bulls not yot at their target area

	char *** HRoutes;
	int i = 0, j, k = 0;
	int bestmatch = 0;

	if (ForwardingInfo->HADDRSP)
	{
		// Match on Routes

		HRoutes = ForwardingInfo->HADDRSP;
		k=0;

		while(HRoutes[k])
		{
			i = j = 0;
			
			while (HRoutes[k][i] && HElements[j]) // Until one set runs out
			{
				if (strcmp(HRoutes[k][i], HElements[j]) != 0)
					break;
				i++;
				j++;
			}

			// Only send if all BBS elements match

			if (HRoutes[k][i] == 0)
			{
				if (i > bestmatch)
						bestmatch = i;
			}
			k++;
		}
	}
	return bestmatch;
}


int CheckBBSHElementsFlood(struct MsgInfo * Msg, struct UserInfo * bbs, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS, char ** HElements)
{
	char *** HRoutes;
	char ** BBSHA;

	int i = 0, j, k = 0;
	int bestmatch = 0;

	if (ForwardingInfo->HADDRS)
	{
		// Match on Routes

		// Message must be in right area (all elements of message match BBS Location HA)

		BBSHA = ForwardingInfo->BBSHAElements;

		if (BBSHA == NULL)
			return 0;				// Not safe to flood
			
		i = j = 0;
			
		while (BBSHA[i] && HElements[j]) // Until one set runs out
		{
			if (strcmp(BBSHA[i], HElements[j]) != 0)
				break;
			i++;
			j++;
		}
		
		if (HElements[j] != 0)
			return 0;				// Message is not for BBS's area 

		HRoutes = ForwardingInfo->HADDRS;
		k=0;

		while(HRoutes[k])
		{
			i = j = 0;
			
			while (HRoutes[k][i] && HElements[j]) // Until one set runs out
			{
				if (strcmp(HRoutes[k][i], HElements[j]) != 0)
					break;
				i++;
				j++;
			}
				
			if (i > bestmatch)
			{
				// As Flooding, only match if all elements match, and elements matching > offset
				
				// As Flooding, only match if all elements from BBS are matched
				// ie if BBS has #23.gbr.eu, and msg gbr.eu, don't match
				// if BBS has gbr.eu, and msg #23.gbr.eu, ok (so long as bbs in in #23, checked above.


				if (HRoutes[k][i] == 0)
					bestmatch = i;
			}
			k++;
		}
	}
	return bestmatch;
}


int CheckBBSToForNTS(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo)
{
	char ** Calls;
	char * Call;
	char * ptr;
	int bestmatch = -1;
	int MatchLen = 0;

	// Look for Matches on TO using Wildcarded Addresses. Intended for use with NTS traffic, with TO = ZIPCode
	
	// We forward to the BBS with the most specific match - ie minimum *'s in match

	if (ForwardingInfo->TOCalls)
	{
		Calls = ForwardingInfo->TOCalls;

		while(Calls[0])
		{
			BOOL Invert = FALSE;		// !Match

			Call = Calls[0];

			if (Call[0] == '!' || Call[0] == '-')
			{
				Call++;
				Invert = TRUE;
			}

			ptr = strchr(Call, '*');

			if (ptr)
			{
				MatchLen = ptr - Call;

				if (memcmp(Msg->to, Call, MatchLen) == 0)
				{
					// Match - de we have a better one?

					// if it is a !Match, return without checking any more

					if (Invert)
						return -1;

					if (MatchLen > bestmatch)
						bestmatch = MatchLen;
				}
			}
			else
			{
				//no star - just do a normal compare
				
				if (strcmp(Msg->to, Call) == 0)
				{
					if (Invert)
						return -1;

					MatchLen = strlen(Call);
					if (MatchLen > bestmatch)
						bestmatch = MatchLen;
				}
			}

			Calls++;
		}
	}
	return bestmatch;
}


int CheckBBSATListWildCarded(struct MsgInfo * Msg, struct BBSForwardingInfo * ForwardingInfo, char * ATBBS)
{
	char ** Calls;
	char * Call;
	char * ptr;
	int bestmatch = -1;
	int MatchLen = 0;

	// Look for Matches on AT using Wildcarded Addresses. Only applied after all other checks fail. Intended mainly
	// for setting a default route, but could have other uses
	
	// We forward to the BBS with the most specific match - ie minimum *'s in match

	if (ForwardingInfo->ATCalls)
	{
		Calls = ForwardingInfo->ATCalls;

		while(Calls[0])
		{
			Call = Calls[0];
			ptr = strchr(Call, '*');

			// only look if * present - we have already tried routing on the full AT
			
			if (ptr)
			{
				MatchLen = ptr - Call;

				if (memcmp(ATBBS, Call, MatchLen) == 0)
				{
					// Match - de we have a better one?

					if (MatchLen > bestmatch)
						bestmatch = MatchLen;
				}
			}

			Calls++;
		}
	}
	return bestmatch;
}

struct ALIAS * CheckForNTSAlias(struct MsgInfo * Msg, char * ATFirstElement)
{
	struct ALIAS ** Alias = NTSAliases;
	char * Call;
	char * ptr;
	int MatchLen = 0;

	//	We have a list of wildcarded TO (ie Zip Codes) and corresponding replacement NTSXX

	//	It seems the NTS people want to match on either TO or DEST, and replace the AT
	//	and to use only the first matching

	while(Alias[0])
	{
		Call = Alias[0]->Dest;

		if (Call == NULL)
			break;

		ptr = strchr(Call, '*');
	
		if (ptr)
		{
			MatchLen = ptr - Call;

			if (memcmp(Msg->to, Call, MatchLen) == 0)
				return(Alias[0]);
		}
		else
		{
			//no star - just do a normal compare
				
			if (strcmp(Msg->to, Call) == 0)
				return(Alias[0]);
		}

		// Try AT

		if (ATFirstElement && ATFirstElement[0])
		{
			if (ptr)
				if (memcmp(ATFirstElement, Call, MatchLen) == 0)
					return(Alias[0]);
			else
				if (strcmp(ATFirstElement, Call) == 0)
					return(Alias[0]);
		}

		Alias++;
	}

	return NULL;
}


VOID ReRouteMessages()
{
	// Pass all messages to the Mail Routing routine.

	// Used if a new BBS is set up, or to readdress messages from a failed BBS

	struct MsgInfo * Msg;
	int n;
	char SaveStatus;
	char Savefbbs[NBMASK];

	for (n = 1; n <= NumberofMessages; n++)
	{
		Msg = MsgHddrPtr[n];

		// if killed, or forwarded and not a bull, ignore

		// If status was F and we add anther BBS, set back to N

		if (Msg->status == 'K' || (Msg->status == 'F' &&  Msg->type != 'B'))
			continue;


		if (Msg->type == 'B')
			SaveStatus = Msg->status;

		memcpy(Savefbbs, Msg->fbbs, NBMASK);

		memset(Msg->fbbs, 0, NBMASK);

		MatchMessagetoBBSList(Msg, NULL);

		if (memcmp(Savefbbs, Msg->fbbs, NBMASK) != 0)
		{
			// Have changed Message Routing

			// Clear fwd bits on any BBS it has been sent to

			if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
			{	
				struct UserInfo * user;

				for (user = BBSChain; user; user = user->BBSNext)
				{
					if (check_fwd_bit(Msg->fbbs, user->BBSNumber))		// for this BBS?	
					{
						if (check_fwd_bit(Msg->forw, user->BBSNumber))	// Already sent?
							 clear_fwd_bit(Msg->fbbs, user->BBSNumber);

					}
				}

				// If still some bits set, change to $

				if (memcmp(Msg->fbbs, zeros, NBMASK) != 0)
				{
					if (FirstMessageIndextoForward > n)
						FirstMessageIndextoForward = n;

					if (Msg->type == 'B')
						Msg->status = '$';
				}
				else
				{
					// all clear - set to N or F

					if (Msg->type == 'B')
					{
						if (memcmp(Msg->forw, zeros, NBMASK) == 0)
							Msg->status = 'N';						// Not sent anywhere, and nowhere to send, so N
						else
							Msg->status = 'F';
					}
				}
			}
		}
	}
}




