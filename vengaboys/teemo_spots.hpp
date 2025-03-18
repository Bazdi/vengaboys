//#pragma once
//#include "math.hpp"
//#include <string>
//#include <vector>
//
//namespace teemo {
//    // Simple struct to store mushroom spot data
//    struct MushroomSpot {
//        std::string name;
//        math::vector3 position;
//        bool enabled = true;
//    };
//
//    // Hard-coded list of mushroom spots for easy editing
//    inline std::vector<MushroomSpot> default_mushroom_spots = {
//        // Dragon pit spots
//        {"Dragon Pit", math::vector3(9866.0f, -71.24f, 4414.0f), true},
//        {"Dragon Bush", math::vector3(10072.0f, -71.24f, 3908.0f), true},
//        {"Blue Mid River", math::vector3(8954.0f, -71.24f, 5458.0f), true},
//
//        // Baron pit spots
//        {"Baron Pit", math::vector3(4866.0f, -71.24f, 10862.0f), true},
//        {"Baron Bush", math::vector3(4526.0f, -71.24f, 11202.0f), true},
//        {"Red Mid River", math::vector3(5926.0f, -71.24f, 8654.0f), true},
//
//        // Mid lane spots
//        {"Mid Lane Bush", math::vector3(6866.0f, -71.24f, 6868.0f), true},
//        {"Mid Lane Side", math::vector3(8108.0f, -71.24f, 6458.0f), true},
//
//        // Top lane spots
//        {"Top Tri Bush", math::vector3(2270.0f, -71.24f, 13160.0f), true},
//        {"Blue Top Bush", math::vector3(2720.0f, -71.24f, 11780.0f), true},
//        {"Red Top Bush", math::vector3(12726.0f, -71.24f, 2954.0f), true},
//
//        // Bot lane spots
//        {"Bot Tri Bush", math::vector3(13200.0f, -71.24f, 2800.0f), true},
//        {"Blue Bot Bush", math::vector3(3658.0f, -71.24f, 1158.0f), true},
//        {"Red Bot Bush", math::vector3(11408.0f, -71.24f, 3952.0f), true},
//
//        // Jungle spots
//        {"Blue Side Blue", math::vector3(3822.0f, -71.24f, 7968.0f), true},
//        {"Blue Side Red", math::vector3(7862.0f, -71.24f, 4112.0f), true},
//        {"Blue Side Wolves", math::vector3(3774.0f, -71.24f, 6504.0f), true},
//        {"Blue Side Gromp", math::vector3(2124.0f, -71.24f, 8306.0f), true},
//        {"Red Side Blue", math::vector3(11160.0f, -71.24f, 6842.0f), true},
//        {"Red Side Red", math::vector3(7120.0f, -71.24f, 10856.0f), true},
//        {"Red Side Wolves", math::vector3(11008.0f, -71.24f, 8320.0f), true},
//        {"Red Side Gromp", math::vector3(12726.0f, -71.24f, 6740.0f), true},
//
//        // Chokepoints
//        {"Mid-Blue Junction", math::vector3(6320.0f, -71.24f, 8400.0f), true},
//        {"Mid-Red Junction", math::vector3(8602.0f, -71.24f, 6540.0f), true},
//        {"Blue Jungle Entrance", math::vector3(5200.0f, -71.24f, 7852.0f), true},
//        {"Red Jungle Entrance", math::vector3(9702.0f, -71.24f, 7108.0f), true}
//    };
//}