function normal		(shader, t_base, t_second, t_detail)
	shader	: begin	("stub_default","stub_default")
			: zb	(true,false)
			: blend	(true,blend.destcolor,blend.one)
--	TODO: DX10: implement aref for this shader
--			: aref 		(true,2)

	shader:sampler	("s_base")      :texture	(t_base)
--	shader: dx10color_write_enable( true, true, true, false)
end