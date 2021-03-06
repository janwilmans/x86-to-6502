#include <cstdint>
#include <array>
#include <utility>
#include <algorithm>
#include <tuple>


namespace {
  volatile uint8_t &memory(const uint16_t loc)
  {
    return *reinterpret_cast<uint8_t *>(loc);
  }

  template<typename T>
    auto square(T t) {
      return t * t;
  }

  constexpr bool test_bit(const uint8_t data, const uint8_t bit)
  {
    return (data & (1 << bit)) != 0;
  };
  
  struct Color
  {
    uint8_t num;
    uint8_t r;
    uint8_t g;
    uint8_t b;
  };

  struct JoyStick
  {
    static constexpr uint16_t JOYSTICK_PORT_A = 56320; // joystick #2
    static constexpr uint16_t JOYSTICK_PORT_B = 56321; // joystick #1
    
    struct PortData
    {
      uint8_t data;
    };
    
    JoyStick(const PortData d)
      : up(!test_bit(d.data, 0)),
        down(!test_bit(d.data, 1)),
        left(!test_bit(d.data, 2)),
        right(!test_bit(d.data, 3)),
        fire(!test_bit(d.data, 4))
    {
    }
    
    JoyStick(const uint8_t port_num)
      : JoyStick(PortData{port_num==2?memory(JOYSTICK_PORT_A):memory(JOYSTICK_PORT_B)})
    {
    }
    
    auto direction_vector() const
    {
      return std::make_pair(left?-1:(right?1:0), up?-1:(down?1:0));
    }
    
    bool up,down,left,right,fire;
  };

  ///
  /// New Code
  ///
  template<typename L1, typename L2, typename R1, typename R2>
    auto operator+=(std::pair<L1&, L2&> lhs, const std::pair<R1, R2> &rhs)
  {
    lhs.first  += rhs.first;
    lhs.second += rhs.second;
    return lhs;
  }

  template<typename L1, typename L2, typename R1, typename R2>
    decltype(auto) operator*=(std::pair<L1, L2> &lhs, const std::pair<R1, R2> &rhs)
  {
    lhs.first  *= rhs.first;
    lhs.second *= rhs.second;
    return (lhs);
  }

  void set_bit(const uint16_t loc, const uint8_t bitnum, bool val)
  {
    if (val) {
      memory(loc) |= (1 << bitnum);
    } else {
      memory(loc) &= (0xFF ^ (1 << bitnum));
    }
  }
  
  struct Player
  {
    Player(const uint8_t num, std::pair<volatile uint8_t &, volatile uint8_t &> sprite_pos,
                              const std::pair<uint8_t, uint8_t> &start_pos)
      : player_num(num),
        pos(sprite_pos)
    {
      pos = start_pos;
    }
    
    void update_position()
    {
      pos.second += JoyStick(player_num).direction_vector().second * 3;
    };

    void scored() {
      ++score;
    }

    const uint8_t player_num;
    std::pair<volatile uint8_t &, volatile uint8_t &> pos;
    uint8_t score = '0';
  };
  ///
  /// End New Code
  ///


  
  
  struct VIC_II
  {
    static constexpr uint8_t  SPRITE_ALIGNMENT          = 64;
    static constexpr uint16_t SPRITE_DATA_POINTERS      = 2040;
    static constexpr uint16_t VIDEO_REGISTERS           = 53248;
    static constexpr uint16_t VIDEO_MEMORY              = 1024;
    static constexpr uint16_t BORDER_COLOR              = 53280;
    static constexpr uint16_t BACKGROUND_COLOR          = 53281;
    static constexpr uint16_t SPRITE_POSITION_REGISTERS = VIDEO_REGISTERS;
    static constexpr uint16_t SPRITE_ENABLE_BITS        = VIDEO_REGISTERS + 21;
    static constexpr uint16_t SPRITE_EXPAND_VERTICAL    = VIDEO_REGISTERS + 23;
    static constexpr uint16_t SPRITE_PRIORITY           = VIDEO_REGISTERS + 27;
    static constexpr uint16_t SPRITE_MULTICOLOR         = VIDEO_REGISTERS + 28;
    static constexpr uint16_t SPRITE_EXPAND_HORIZONTAL  = VIDEO_REGISTERS + 29;
    static constexpr uint16_t SPRITE_COLLISIONS         = VIDEO_REGISTERS + 30;
    static constexpr uint16_t SPRITE_0_COLOR            = VIDEO_REGISTERS + 39;
    static constexpr uint16_t SPRITE_1_COLOR            = SPRITE_0_COLOR + 1;
    static constexpr uint16_t SPRITE_2_COLOR            = SPRITE_1_COLOR + 1;
    static constexpr uint16_t SCREEN_RASTER_LINE        = 53266;

    
    volatile uint8_t& border() {
      return memory(BORDER_COLOR);
    }
    
    volatile uint8_t& background() {
      return memory(BACKGROUND_COLOR);
    }
    
    volatile uint8_t& display(uint8_t x, uint8_t y)
    {
      return memory(VIDEO_MEMORY + y * 40 + x);
    }
    
    template<uint8_t r, uint8_t g, uint8_t b>
    static auto color_comparison(const Color &lhs, const Color &rhs)
    {
      // distance between colors:
      // sqrt( (r1 - r2)^2 + (g1 - g2)^2 + (b1 - b2)^2 )
      return (square(lhs.r - r) + square(lhs.g - g) + square(lhs.b - b))
           < (square(rhs.r - r) + square(rhs.g - g) + square(rhs.b - b));
    }

    template<uint8_t r, uint8_t g, uint8_t b, typename Colors>
    static auto nearest_color(const Colors &colors)
    {
      return *std::min_element(std::begin(colors), std::end(colors), 
                               color_comparison<r,g,b>);
    }
    
    auto frame(Player &p1, Player &p2)
    {
      struct Frame
      {
        Frame(VIC_II &t_vic, Player &p1, Player &p2) 
          : player1(p1), player2(p2), vic(t_vic)
        {
          while (memory(SCREEN_RASTER_LINE) != 250) {}
        }
        
        ~Frame() {
          vic.display(10, 12) = player1.score;
          vic.display(20, 12) = player2.score;
        }
        
        Player &player1;
        Player &player2;
        VIC_II &vic;
      };
      
      return Frame(*this, p1, p2);
    }

    template<bool multicolor>
    struct SpriteLine {
      uint8_t pixels[3];

      typedef uint_least64_t numeric_t;

      /// Pixel format converter.
      static constexpr uint8_t c(const numeric_t m)
      {
        if constexpr (multicolor)
          // 0b00ii00jj00kk00ll -> 0biijjkkll
         return uint8_t(m >> 0 & 3 << 0) |
                uint8_t(m >> 2 & 3 << 2) |
                uint8_t(m >> 4 & 3 << 4) |
                uint8_t(m >> 6 & 3 << 6);
        return uint8_t(m);
      }

      /// Input bits per byte.
      constexpr static auto ibb = multicolor ? 16 : 8;

      /// Constructor.
      constexpr SpriteLine(const numeric_t line)
        : pixels{c(line >> (2 * ibb)), c(line >> ibb), c(line)}
      {}
    };

    template<bool multicolor>
    struct Sprite {
      typedef SpriteLine<multicolor> Line;
      alignas(SPRITE_ALIGNMENT) Line lines[SPRITE_ALIGNMENT / sizeof(Line)];
    };

    typedef Sprite<false> HighResSprite;
    typedef Sprite<true> MultiColorSprite;

    ///
    /// New Code
    ///
    template<bool multicolor>
    void enable_sprite(const uint8_t sprite_number,
                       const Sprite<multicolor>& bitmap,
                       const bool low_priority,
                       const bool double_width, const bool double_height)
    {
#if 0 // error: static_assert expression is not an integral constant
      static_assert((std::ptrdiff_t(bitmap.memory) & 0x7000) != 0x1000,
		    "The addresses 0x1000 to 0x1fff and 0x9000 to 0x9fff"
		    "point to the character generator ROM, not RAM.");
      static_assert(std::ptrdiff_t(bitmap.memory) < 0x4000,
		    "The data must be within the first (default)"
		    " 16KiB VIC-II bank.");
#endif
      memory(SPRITE_DATA_POINTERS + sprite_number)
        = (std::ptrdiff_t(bitmap.lines) & 0x3fff) / SPRITE_ALIGNMENT;
      set_bit(SPRITE_ENABLE_BITS, sprite_number, true);
      set_bit(SPRITE_EXPAND_HORIZONTAL, sprite_number, double_width);
      set_bit(SPRITE_EXPAND_VERTICAL, sprite_number, double_height);
      set_bit(SPRITE_MULTICOLOR, sprite_number, multicolor);
      set_bit(SPRITE_PRIORITY, sprite_number, low_priority);
    }

    auto sprite_collisions() {
      const auto collisions = memory(SPRITE_COLLISIONS);
      
      return std::make_tuple(
        test_bit(collisions, 0),test_bit(collisions, 1),test_bit(collisions, 2),
        test_bit(collisions, 3),test_bit(collisions, 4),test_bit(collisions, 5),
        test_bit(collisions, 6),test_bit(collisions, 7));
    }
    
    volatile uint8_t &sprite_1_color()
    {
      return memory(SPRITE_1_COLOR);
    }

    volatile uint8_t &sprite_2_color()
    {
      return memory(SPRITE_2_COLOR);
    }
    
    std::pair<volatile uint8_t &, volatile uint8_t &> 
      sprite_pos(const uint8_t sprite_num)
    {
      return {
               memory(SPRITE_POSITION_REGISTERS + sprite_num * 2),
               memory(SPRITE_POSITION_REGISTERS + sprite_num * 2 + 1)
             };
    }
    ///
    /// End New Code
    ///


  };  

}

int main()
{
  const std::array<Color, 16> colors = {{
    Color{0,  0x00, 0x00, 0x00},
    Color{1,  0xFF, 0xFF, 0xFF},
    Color{2,  0x88, 0x39, 0x32},
    Color{3,  0x67, 0xB6, 0xBD},
    Color{4,  0x8B, 0x3F, 0x96},
    Color{5,  0x55, 0xA0, 0x49},
    Color{6,  0x40, 0x31, 0x8D},
    Color{7,  0xBF, 0xCE, 0x72},
    Color{8,  0x8B, 0x54, 0x29},
    Color{9,  0x57, 0x42, 0x00},
    Color{10, 0xB8, 0x69, 0x62},
    Color{11, 0x50, 0x50, 0x50},
    Color{12, 0x78, 0x78, 0x78},
    Color{13, 0x94, 0xE0, 0x89},
    Color{14, 0x78, 0x69, 0xC4},
    Color{15, 0x9F, 0x9F, 0x9F}
  }};

  VIC_II vic;

  /// The ball image.
  static const VIC_II::HighResSprite sBall
  {{
      0b000000000000000000000000,
      0b000000000000000000000000,
      0b000000000111111000000000,
      0b000000011111111110000000,
      0b000000110111111111000000,
      0b000001100011111111100000,
      0b000001110111111111100000,
      0b000011111111111111110000,
      0b000011111111111111110000,
      0b000011111111111111110000,
      0b000000111111111111000000,
      0b000011000000000000110000,
      0b000011111111111111110000,
      0b000001111111111111100000,
      0b000001111111111111100000,
      0b000000111111111111000000,
      0b000000011111111110000000,
      0b000000000111111000000000,
      0b000000000000000000000000,
      0b000000000000000000000000,
      0b000000000000000000000000
  }};

  /// The bat image.
  static const VIC_II::MultiColorSprite sBat = {{
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000002200000,
    0x000003300000,
    0x000001100000,
    0x000003300000,
    0x000001100000,
    0x000003300000
  }};

  vic.enable_sprite(0, sBall, true, false, false);
  vic.enable_sprite(1, sBat, false, false, true);
  vic.enable_sprite(2, sBat, false, false, true);
  vic.border()         = vic.nearest_color<128,128,128>(colors).num; // 50% grey
  vic.background()     = vic.nearest_color<0,0,0>(colors).num;       // black
  vic.sprite_1_color() = vic.nearest_color<255,0,0>(colors).num;     // red
  vic.sprite_2_color() = vic.nearest_color<0,255,0>(colors).num;     // green

  
  ///
  /// Game Logic
  ///
  std::pair<int8_t, int8_t> ball_velocity{1,1};

  const auto reset_ball = [&vic]{
    // resets position but keeps velocity
    vic.sprite_pos(0) = std::make_pair(255/2, 255/2);
  };

  reset_ball();
  
  Player p1(1, vic.sprite_pos(1), {15,  255/2});
  Player p2(2, vic.sprite_pos(2), {255, 255/2});
  
  ///
  /// Game Loop
  ///
  while (true) {    
    auto frame = vic.frame(p1, p2);
    
    if (const auto [s0, s1, s2, s3, s4, s5, s6, s7] = vic.sprite_collisions();
        s0 && (s1 || s2))
    {
      // ball hit paddle, invert ball x velocity
      ball_velocity *= std::make_pair(-1, 1);
      // "bounce" ball out of collision area
      vic.sprite_pos(0) += std::make_pair(ball_velocity.first, 0);
    }

    
    // Update paddle positions
    p1.update_position();
    p2.update_position();

    
    const auto score = [reset_ball](auto &player){
      // called when a player scores
      player.scored();
      reset_ball();
    };

    if (const auto [ball_x, ball_y] = vic.sprite_pos(0) += ball_velocity;
        ball_y == 45 || ball_y == 235) 
    {
      // ball hit the top or bottom wall, invert ball y velocity
      ball_velocity *= std::make_pair(1, -1);
    } else if (ball_x == 1) {	
      // ball hit left wall, player 2 scored
      score(p2);
    } else if (ball_x == 255) {
      // ball hit right wall, player 1 scored
      score(p1);
    }
  }  
}

