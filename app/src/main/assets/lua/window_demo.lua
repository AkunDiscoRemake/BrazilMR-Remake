-- BrazilMR SDK Lua — exemplo "Janela Espacial"
-- Demonstra: criação de janelas 3D, transform dinâmico e raycast.

local WIN = 500

vr.createWindow(WIN, 0.8, 0.5, -0.9, 0.15, -1.8)

-- anima a janela num arco suave (senoide)
local t = 0.0
vr.onEvent(function(type, a, b, x, y, z)
  -- type 6 = WINDOW_INPUT (clique dentro da janela)
  if type == 6 and a == WIN then
    vr.log("janela tocada em u=" .. string.format("%.2f", x))
  end
end)

-- o próprio script pode ser reiniciado; guardamos t num global simples
vr.log("window_demo.lua carregado")

function tick(dt)
  t = t + dt
  local sway = math.sin(t * 0.8) * 0.12
  local bob  = math.sin(t * 1.1) * 0.04
  local yaw  = math.sin(t * 0.5) * 8.0
  vr.setWindowTransform(WIN, -0.9 + sway, 0.15 + bob, -1.8, yaw, 1.0)
end
