package moe.crx.overport.galaxyxr

/** QCOM quality constraints. Defaults are provisional tuning values, not measured optima. */
data class FoveationSettings(val density: Float = 0.35f, val gain: Float = 24f, val area: Float = 0.15f) {
    init {
        require(density.isFinite() && density in 0.125f..1f) { "Foveation density must be 0.125..1." }
        require(gain.isFinite() && gain in 0f..32f) { "Foveation gain must be 0..32." }
        require(area.isFinite() && area in 0f..2f) { "Foveation area must be 0..2." }
    }
    fun arguments() = listOf(density.toString(), gain.toString(), area.toString())
    fun config() = "qcomDensity=$density\nqcomGain=$gain\nqcomArea=$area\n"
    companion object {
        fun parse(density: String?, gain: String?, area: String?): FoveationSettings {
            fun value(text: String?, fallback: Float): Float =
                if (text == null) fallback else text.toFloatOrNull() ?: error("Invalid foveation number: $text")
            return FoveationSettings(value(density, .35f), value(gain, 24f), value(area, .15f))
        }
    }
}
