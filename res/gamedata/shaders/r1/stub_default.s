function normal		(shader, t_base, t_second, t_detail)
	shader:begin	("stub_default","stub_default")

--	Decouple sampler and texture
	shader:sampler	("s_base")	: texture(t_base)	: clamp() : f_linear ()
end