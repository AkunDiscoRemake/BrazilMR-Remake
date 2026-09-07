-- BrazilMR SDK Lua — exemplo "Olá VR"
-- Demonstra: modelos GLB, física nativa, gestos e pose da cabeça.

local logo = vr.loadModel("models/brazilmr_logo.glb")
if logo ~= 0 then
  vr.setModelTransform(logo, 0.0, 0.55, -1.9, 15.0, 0.4, true)
  vr.setModelAnimation(logo, 0, true)
end

-- esferas com física (gravidade + colisão do motor nativo)
local bolas = {}
local cores = {
  {0.0, 0.9, 0.63}, {0.95, 0.75, 0.1}, {0.35, 0.55, 0.95},
  {0.9, 0.3, 0.4},  {0.7, 0.5, 0.9},
}
for i = 1, 5 do
  local id = vr.spawnSphere(-0.8 + i * 0.4, 1.6 + i * 0.2, -2.4, 0.075)
  vr.setColor(id, cores[i][1], cores[i][2], cores[i][3], 1.0)
  bolas[i] = id
end

-- reage a gestos: palma aberta limpa a cena; pinça cria esfera
vr.onGesture(function(hand, gesture, strength)
  if gesture == "OPEN_PALM" then
    for _, id in ipairs(bolas) do
      vr.removeObject(id)
    end
    bolas = {}
    vr.log("cena limpa")
  elseif gesture == "PINCH" then
    local pose = vr.getHeadPose()
    local id = vr.spawnSphere(pose[1], pose[2] - 0.3, pose[3] - 1.0, 0.06)
    vr.setColor(id, 1.0, 1.0, 1.0, 1.0)
    table.insert(bolas, id)
    vr.log("esfera criada")
  end
end)

vr.log("hello.lua carregado")
